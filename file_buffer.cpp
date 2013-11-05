// Copyright 2007 Edd Dawson.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file BOOST_LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include "file_buffer.h"

#include <algorithm>
#include <cstring>

using std::size_t;

FILE_buffer::FILE_buffer(string fileName, vector<size_t> offsets,
					 vector<size_t> lengths, size_t buff_sz, size_t put_back) :
	put_back_(std::max(put_back, size_t(1))),
	buffer_(std::max(buff_sz, put_back_) + put_back_),
	segmentOffsets(offsets),
	segmentLengths(lengths)

{
	fptr_ = fopen(fileName.c_str(), "rb");
	reset();
}

FILE_buffer::~FILE_buffer()
{
	if (fptr_)
		fclose(fptr_);

}


void FILE_buffer::reset()
{
	// Set the back, current and end buffer pointers to be equal.
	// This will force an underflow() on the first read and hence
	// fill the buffer.
	char *end = &buffer_.front() + buffer_.size();
	setg(end, end, end);
	init();

}

std::streambuf::int_type FILE_buffer::underflow()
{
	if (gptr() < egptr()) // buffer not exhausted
		return traits_type::to_int_type(*gptr());

	char *base = &buffer_.front();
	char *start = base;

	if (eback() == base)
	{
		// Make arrangements for putback characters
		std::memmove(base, egptr() - put_back_, put_back_);
		start += put_back_;
	}

	// start is now the start of the buffer, proper.
	// Read from fptr_ in to the provided buffer
	size_t n = read(start, buffer_.size() - (start - base));
	if (n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);
	
	return traits_type::to_int_type(*gptr());
}

size_t FILE_buffer::getLength() 
{
	size_t len = 0;
	int i = 0;
	for (i = 0; i < segmentOffsets.size(); i++) {
		len += segmentLengths[i];
    }
    return len;
}
void  FILE_buffer::incrementSegment()
{
	if ( curSegment == segmentOffsets.size()-1)
		return;

	if (curPos == (segmentOffsets[curSegment] + segmentLengths[curSegment]))
	{
		curSegment++;
		if (curSegment < segmentOffsets.size())
		{
			curPos = segmentOffsets[curSegment];
			if (fseek(fptr_,curPos,SEEK_SET)) 
			{
				return;
			}
		}
	}
}

size_t  FILE_buffer::read (void * p_buffer, size_t p_nb_bytes)
{
	size_t nb_read;
	size_t bytesInCurrentSegment;
	size_t bytesToRead;
	size_t totalBytesRead;
	size_t bytesLeftToRead;
	size_t bytesRemainingInFile;

	if (p_buffer == NULL || p_nb_bytes == 0 )
		return 0;

	//don't try to read more bytes than are available
	bytesRemainingInFile = dataLength - dataRead;
	if (p_nb_bytes > bytesRemainingInFile)
		p_nb_bytes = bytesRemainingInFile;

	totalBytesRead = 0;
	bytesLeftToRead = p_nb_bytes;
	while (bytesLeftToRead > 0 && curSegment < segmentOffsets.size())
	{
		bytesInCurrentSegment = segmentOffsets[curSegment]  +
									segmentLengths[curSegment]  -  
									   curPos;

		bytesToRead = (bytesLeftToRead < bytesInCurrentSegment)  ?  bytesLeftToRead  :  bytesInCurrentSegment;
		nb_read = fread( (unsigned char*)p_buffer+totalBytesRead,1,bytesToRead,fptr_);
		curPos += nb_read;
		dataRead += nb_read;
		totalBytesRead += nb_read;
		bytesLeftToRead -= nb_read;

		incrementSegment();
		if (!nb_read)
			break;
	}
	return totalBytesRead ? totalBytesRead : (size_t)-1;
}



//assume that segment info has been already set
bool  FILE_buffer::init ()
{

	curPos = segmentOffsets[0];
	curSegment = 0;
	dataLength = getLength();
	dataRead = 0;

	if (fptr_)
	{
		if (fseek(fptr_,curPos,SEEK_SET)) 
		{
			return false;
		}
	}
	return true;
}

