#include "typed-bytes.h"
#include <deque>
#include <iostream>

typedef std::deque<unsigned char> raw;
#include <string>
#include <iostream>

class ReadPastEnd {
  public:
    ReadPastEnd(){};
};

int raw2int(const raw & data, int & start) {
  if(data.size() < start + 4) {
    throw ReadPastEnd();}
  int retval = 0;
  for (int i = 0; i < 4; i ++) {
    retval = retval + ((data[start + i] & 255) << (8*(3 - i)));}
  start = start + 4;
  return retval;}

long raw2long(const raw & data, int & start) {
  if(data.size() < start + 8) {
    throw ReadPastEnd();  }
  long retval = 0;
  for(int i = 0; i < 8; i++) {
    retval = retval + (((long long) data[start + i] & 255) << (8*(7 - i)));}
  start = start + 8; 
return retval;}

double raw2double(const raw & data, int & start) {
  union udouble {
    double d;
    unsigned long u;} ud;
  ud.u = raw2long(data, start);
  return ud.d;} 

int get_length(const raw & data, int & start) {
  return raw2int(data, start);}

unsigned char get_type(const raw & data, int & start) {
  if(data.size() < start + 1) {
    throw ReadPastEnd();}
  unsigned char retval  = data[start];
  start = start + 1;
  return retval;}

int unserialize(const raw & data, int & start, Rcpp::List & objs){
  unsigned char type_code = get_type(data, start);
  switch(type_code) {
    case 0: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      raw tmp(data.begin() + start, data.begin() + start + length);
      objs.insert(objs.end(), Rcpp::wrap(tmp));
      start = start + length;}
    break;
    case 1: {
      if(data.size() < start + 1) {
        throw ReadPastEnd();}
      objs.push_back(Rcpp::wrap((unsigned char)(data[start])));
      start = start + 1; }
    break;
    case 2: {
      if(data.size() < start + 1) {
        throw ReadPastEnd();}
      objs.push_back(Rcpp::wrap(bool(data[start])));
      start = start + 1;}
    break;
    case 3: {
      objs.push_back(Rcpp::wrap(raw2int(data, start)));}      
    break;
    case 4:
    case 5: {
      std::cerr << type_code << " type code not supported" << std::endl;}
    break;
    case 6: {
      objs.push_back(Rcpp::wrap(raw2double(data, start)));}
    break;
    case 7: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      std::string tmp(data.begin() + start, data.begin() + start + length);
      objs.insert(objs.end(), Rcpp::wrap(tmp));
      start = start + length;}
    break;
    case 8: {
      int length = get_length(data, start);
      Rcpp::List l(0);
      for(int i = 0; i < length; i++) {
        unserialize(data, start, l);}
      objs.push_back(Rcpp::wrap(l));}
    break;
    case 9: 
    case 10: {
      std::cerr << type_code << " type code not supported" << std::endl;}
    break;
    case 144: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      Rcpp::Function r_unserialize("unserialize");
      raw tmp(data.begin() + start, data.begin() + start + length);
      objs.insert(objs.end(), r_unserialize(Rcpp::wrap(tmp)));
      start = start + length;}
    break;
    default: {
      std::cerr << "Unknown type code " << type_code << std::endl;}}}


SEXP typed_bytes_reader(SEXP data){
	Rcpp::List objs(0);
	Rcpp::RawVector tmp(data);
	raw rd(tmp.begin(), tmp.end());
	int start = 0;
	int current_start = 0;
	while(rd.size() > start) {
 		try{
    			unserialize(rd, start, objs);
    			current_start = start;}
  		catch (ReadPastEnd rpe){
    			std::cerr << "Incomplete objects parsed" << std::endl;
    			break;}}
	return Rcpp::wrap(Rcpp::List::create(
  		Rcpp::Named("objects") = Rcpp::wrap(objs),
  		Rcpp::Named("parsed.length") = Rcpp::wrap(current_start)));}




raw T2raw(unsigned char data) {
  raw serialized(1);
  serialized[0] = data;
  return serialized;}

raw T2raw(int data) {
  raw serialized(0);
  for(int i = 0; i < 4; i++) {
    serialized.push_back((data >> (8*(3 - i))) & 255);}
  return serialized;}

raw T2raw(unsigned long data) {
  raw serialized(0);
  for(int i = 0; i < 8; i++) {
    serialized.push_back((data >> (8*(7 - i))) & 255);}
  return serialized;}

raw T2raw(double data) {
  union udouble {
    double d;
    unsigned long u;} ud;
  ud.d = data;
  return T2raw(ud.u);}

raw length_header(int l){
  return T2raw(l);}

template <typename T> void serialize_one(const T & data, unsigned char type_code, raw & serialized) {
  serialized.push_back(type_code);
  raw rd = T2raw(data);
  serialized.insert(serialized.end(), rd.begin(), rd.end());}

template <typename T> void serialize_many(const T & data, unsigned char type_code, raw & serialized){
  serialized.push_back(type_code);
  raw lh(length_header(data.size()));
  serialized.insert(serialized.end(),lh.begin(), lh.end());
  serialized.insert(serialized.end(), data.begin(), data.end());}

void serialize(const SEXP & object, raw & serialized); 

template <typename T> void serialize_vector(const T & data, unsigned char type_code, raw & serialized){
  serialized.push_back(8);
  raw lh(length_header(data.size()));
  serialized.insert(serialized.end(), lh.begin(), lh.end());
  for(typename T::iterator i = data.begin(); i < data.end(); i++) {
    serialize_one(*i, type_code, serialized);}}

template <typename T> void serialize_list(const T & data, raw & serialized){
  serialized.push_back(8);
  raw lh(length_header(data.size()));
  serialized.insert(serialized.end(), lh.begin(), lh.end());
  for(typename T::iterator i = data.begin(); i < data.end(); i++) {
    serialize(Rcpp::wrap(*i), serialized);}}

void serialize(const SEXP & object, raw & serialized) {
  Rcpp::RObject robj(object);
  switch(robj.sexp_type()) {
    case 24: {//raw
      Rcpp::RawVector data(object);
      if(data.size() == 1){
        serialize_one(data[0], 1, serialized);}
      else {
        serialize_many(data, 0, serialized);}}
      break;
    case 16: { //character
      Rcpp::CharacterVector data(object);
      if(data.size() == 1) {
        serialize_many(data[0], 7, serialized);}
      else {
        serialized.push_back(8);
        raw lh(length_header(data.size()));
        serialized.insert(serialized.end(), lh.begin(), lh.end());
        for(int i = 0; i < data.size(); i++) {
          serialize_many(data[i], 7, serialized);}}}
      break; 
    case 10: { //logical
      Rcpp::LogicalVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 2, serialized);}
      else {
        serialize_vector(data, 2, serialized);}}
      break;
    case 14: { //numeric
      Rcpp::NumericVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 6, serialized);}
      else {
        serialize_vector(data, 6, serialized);}}
      break;
    case 13: { //factor, integer
      Rcpp::IntegerVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 3, serialized);}
      else {
        serialize_vector(data, 3, serialized);}
      }
      break;
    case 19: { //list
      Rcpp::List data(object);
      serialize_list(data, serialized);}
      break;
    default:
    std::cerr << "object type not supported: " << robj.sexp_type();}}

SEXP typed_bytes_writer(SEXP objs){
	raw serialized(0);
	Rcpp::List objects(objs);
	for(Rcpp::List::iterator i = objects.begin(); i < objects.end(); i++) {
	  serialize(Rcpp::wrap(*i), serialized);}
	return Rcpp::wrap(serialized);}	


