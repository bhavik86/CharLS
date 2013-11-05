// Copyright 2007 Edd Dawson.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file BOOST_LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#ifndef FILE_BUFFER_HPP_1819_13042007_
#define FILE_BUFFER_HPP_1819_13042007_

#include <streambuf>
#include <vector>
#include <cstdlib>

#if defined(__BORLANDC__)
	#include <stdio.h>
#else
	#include <cstdio>
#endif

using namespace std;

class FILE_buffer : public std::streambuf
{
	public:
		explicit FILE_buffer(string fileName,vector<size_t> offsets,
								vector<size_t> lengths,  std::size_t buff_sz = 256, std::size_t put_back = 8);
		void reset();

		virtual ~FILE_buffer();

	private:
		// overrides base class underflow()
		int_type underflow();
		
		// copy ctor and assignment not implemented;
		// copying not allowed
		FILE_buffer(const FILE_buffer &); 
		FILE_buffer &operator= (const FILE_buffer &);

	private:

		size_t getLength();
		void  incrementSegment();
		size_t  read (void * p_buffer, size_t p_nb_bytes);
		bool  init ();

		FILE *fptr_;
		const std::size_t put_back_;
		std::vector<char> buffer_;

		size_t dataLength;
		size_t dataRead;
		std::vector<size_t> segmentOffsets;
		std::vector<size_t> segmentLengths;
		size_t curPos;
		int curSegment;
};

#endif
