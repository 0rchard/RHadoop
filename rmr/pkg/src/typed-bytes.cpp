//Copyright 2011 Revolution Analytics
//   
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS, 
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

#include "typed-bytes.h"
#include <deque>
#include <iostream>

typedef std::deque<unsigned char> raw;
#include <string>
#include <iostream>

class ReadPastEnd {
  public:
      ReadPastEnd(){};};
      
class UnsupportedType{
	public:
		unsigned char type_code;
		UnsupportedType(unsigned char _type_code){
			type_code = _type_code;};};
		
class NegativeLength {
	public:
		NegativeLength(){};};

class FastList {
	public:
		FastList(){
			true_size = 0;}
		Rcpp::List list;
		int true_size;
		void push_back(SEXPREC * ro){
			if(true_size == list.size()) {
				Rcpp::List newlist(2 * list.size() + 1);
				if(true_size > 0) {
					newlist[Rcpp::Range(0, true_size)] = list;}
				list = newlist;}
			list[true_size] = ro;
			true_size = true_size + 1;}};

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
  int len = raw2int(data, start);
  if(len < 0) {
  	throw NegativeLength();}
  return len;}

unsigned char get_type(const raw & data, int & start) {
  if(data.size() < start + 1) {
    throw ReadPastEnd();}
  unsigned char retval  = data[start];
  start = start + 1;
  return retval;}

int unserialize(const raw & data, int & start, FastList & objs){
  unsigned char type_code = get_type(data, start);
  switch(type_code) {
    case 0: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      raw tmp(data.begin() + start, data.begin() + start + length);
      objs.push_back(Rcpp::wrap(tmp));
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
      throw UnsupportedType(type_code);}
    break;
    case 6: {
      objs.push_back(Rcpp::wrap(raw2double(data, start)));}
    break;
    case 7: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      std::string tmp(data.begin() + start, data.begin() + start + length);
      objs.push_back(Rcpp::wrap(tmp));
      start = start + length;}
    break;
    case 8: {
      int length = get_length(data, start);
      FastList l;
      for(int i = 0; i < length; i++) {
        unserialize(data, start, l);}
        Rcpp::List tmp(l.list.begin(), l.list.begin() + l.true_size);
      objs.push_back(Rcpp::wrap(tmp));}
    break;
    case 9: 
    case 10: {
      throw UnsupportedType(type_code);}
    break;
    case 144: {
      int length = get_length(data, start);
      if(data.size() < start + length) {
        throw ReadPastEnd();}
      Rcpp::Function r_unserialize("unserialize");
      raw tmp(data.begin() + start, data.begin() + start + length);
      objs.push_back(r_unserialize(Rcpp::wrap(tmp)));
      start = start + length;}
    break;
    default: {
      throw UnsupportedType(type_code);}}}


SEXP typed_bytes_reader(SEXP data){
	FastList objs;
	Rcpp::RawVector tmp(data);
	raw rd(tmp.begin(), tmp.end());
	int start = 0;
	int current_start = 0;
	while(rd.size() > start) {
 		try{
   			unserialize(rd, start, objs);
   			current_start = start;}
  		catch (ReadPastEnd rpe){
    			break;}
		catch (UnsupportedType ue) {
				std::cerr << "Unsupported type: " << ue.type_code << std::endl;
    			return R_NilValue;}
		catch (NegativeLength nl) {
			    std::cerr << "Negative length Exception" << std::endl;}}
    Rcpp::List list_tmp(objs.list.begin(), objs.list.begin() + objs.true_size);
	return Rcpp::wrap(Rcpp::List::create(
  		Rcpp::Named("objects") = Rcpp::wrap(list_tmp),
  		Rcpp::Named("length") = Rcpp::wrap(current_start)));}




void T2raw(unsigned char data, raw & serialized) {
  serialized.push_back(data);}

void T2raw(int data, raw & serialized) {
  for(int i = 0; i < 4; i++) {
    serialized.push_back((data >> (8*(3 - i))) & 255);}}

void T2raw(unsigned long data, raw & serialized) {
  for(int i = 0; i < 8; i++) {
    serialized.push_back((data >> (8*(7 - i))) & 255);}}

void T2raw(double data, raw & serialized) {
  union udouble {
    double d;
    unsigned long u;} ud;
  ud.d = data;
  T2raw(ud.u, serialized);}

void length_header(int len, raw & serialized){
  if(len < 0) {
  	throw NegativeLength();}
  T2raw(len, serialized);}

template <typename T> void serialize_one(const T & data, unsigned char type_code, raw & serialized) {
  serialized.push_back(type_code);
  T2raw(data, serialized);}

template <typename T> void serialize_many(const T & data, unsigned char type_code, raw & serialized){
  serialized.push_back(type_code);
  length_header(data.size(), serialized);
  serialized.insert(serialized.end(), data.begin(), data.end());}

void serialize(const SEXP & object, raw & serialized); 

template <typename T> void serialize_vector(const T & data, unsigned char type_code, raw & serialized){
  serialized.push_back(8);
  length_header(data.size(), serialized);
  for(typename T::iterator i = data.begin(); i < data.end(); i++) {
    serialize_one(*i, type_code, serialized);}}

template <typename T> void serialize_list(const T & data, raw & serialized){
  serialized.push_back(8);
  length_header(data.size(), serialized);
  for(typename T::iterator i = data.begin(); i < data.end(); i++) {
    serialize(Rcpp::wrap(*i), serialized);}}

void serialize(const SEXP & object, raw & serialized) {
  Rcpp::RObject robj(object);
  switch(robj.sexp_type()) {
  	case NILSXP: {
  	  throw UnsupportedType(NILSXP);}
  	  break;
    case RAWSXP: {//raw
      Rcpp::RawVector data(object);
      if(data.size() == 1){
        serialize_one(data[0], 1, serialized);}
      else {
        serialize_many(data, 0, serialized);}}
      break;
    case STRSXP: { //character
      Rcpp::CharacterVector data(object);
      if(data.size() == 1) {
        serialize_many(data[0], 7, serialized);}
      else {
        serialized.push_back(8);
        length_header(data.size(), serialized);
        for(int i = 0; i < data.size(); i++) {
          serialize_many(data[i], 7, serialized);}}}
      break; 
    case LGLSXP: { //logical
      Rcpp::LogicalVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 2, serialized);}
      else {
        serialize_vector(data, 2, serialized);}}
      break;
    case REALSXP: { //numeric
      Rcpp::NumericVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 6, serialized);}
      else {
        serialize_vector(data, 6, serialized);}}
      break;
    case INTSXP: { //factor, integer
      Rcpp::IntegerVector data(object);
      if(data.size() == 1) {
        serialize_one(data[0], 3, serialized);}
      else {
        serialize_vector(data, 3, serialized);}
      }
      break;
    case VECSXP: { //list
      Rcpp::List data(object);
      serialize_list(data, serialized);}
      break;
    default: {
    throw UnsupportedType(robj.sexp_type());}}}

SEXP typed_bytes_writer(SEXP objs){
	raw serialized(0);
	Rcpp::List objects(objs);
	for(Rcpp::List::iterator i = objects.begin(); i < objects.end(); i++) {
	  	try{
	  		serialize(Rcpp::wrap(*i), serialized);}
  		catch(UnsupportedType ut){
  			return R_NilValue;}}
	return Rcpp::wrap(serialized);}	


