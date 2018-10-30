#include "MultipartParser.hpp"
#include <cstring>
#include "NameValueParser.hpp"





namespace Http {





MultipartParser::MultipartParser(const std::string & aContentType, Callbacks & aCallbacks) :
	mCallbacks(aCallbacks),
	mIsValid(true),
	mEnvelopeParser(*this),
	mHasHadData(false)
{
	// Check that the content type is multipart:
	std::string ContentType(aContentType);
	if (strncmp(ContentType.c_str(), "multipart/", 10) != 0)
	{
		mIsValid = false;
		return;
	}
	size_t idxSC = ContentType.find(';', 10);
	if (idxSC == std::string::npos)
	{
		mIsValid = false;
		return;
	}

	// Find the multipart boundary:
	ContentType.erase(0, idxSC + 1);
	NameValueParser CTParser(ContentType.c_str(), ContentType.size());
	CTParser.finish();
	if (!CTParser.isValid())
	{
		mIsValid = false;
		return;
	}
	mBoundary = CTParser["boundary"];
	mIsValid = !mBoundary.empty();
	if (!mIsValid)
	{
		return;
	}

	// Set the envelope parser for parsing the body, so that our Parse() function parses the ignored prefix data as a body
	mEnvelopeParser.setIsInHeaders(false);

	// Append an initial CRLF to the incoming data, so that a body starting with the boundary line will get caught
	mIncomingData.assign("\r\n");

	/*
	mBoundary = std::string("\r\n--") + mBoundary
	mBoundaryEnd = mBoundary + "--\r\n";
	mBoundary = mBoundary + "\r\n";
	*/
}





void MultipartParser::parse(const char * aData, size_t aSize)
{
	// Skip parsing if invalid
	if (!mIsValid)
	{
		return;
	}

	// Append to buffer, then parse it:
	mIncomingData.append(aData, aSize);
	for (;;)
	{
		if (mEnvelopeParser.isInHeaders())
		{
			size_t BytesConsumed = mEnvelopeParser.parse(mIncomingData.data(), mIncomingData.size());
			if (BytesConsumed == std::string::npos)
			{
				mIsValid = false;
				return;
			}
			if ((BytesConsumed == aSize) && mEnvelopeParser.isInHeaders())
			{
				// All the incoming data has been consumed and still waiting for more
				return;
			}
			mIncomingData.erase(0, BytesConsumed);
		}

		// Search for boundary / boundary end:
		size_t idxBoundary = mIncomingData.find("\r\n--");
		if (idxBoundary == std::string::npos)
		{
			// Boundary string start not present, present as much data to the part callback as possible
			if (mIncomingData.size() > mBoundary.size() + 8)
			{
				size_t BytesToReport = mIncomingData.size() - mBoundary.size() - 8;
				mCallbacks.onPartData(mIncomingData.data(), BytesToReport);
				mIncomingData.erase(0, BytesToReport);
			}
			return;
		}
		if (idxBoundary > 0)
		{
			mCallbacks.onPartData(mIncomingData.data(), idxBoundary);
			mIncomingData.erase(0, idxBoundary);
		}
		idxBoundary = 4;
		size_t LineEnd = mIncomingData.find("\r\n", idxBoundary);
		if (LineEnd == std::string::npos)
		{
			// Not a complete line yet, present as much data to the part callback as possible
			if (mIncomingData.size() > mBoundary.size() + 8)
			{
				size_t BytesToReport = mIncomingData.size() - mBoundary.size() - 8;
				mCallbacks.onPartData(mIncomingData.data(), BytesToReport);
				mIncomingData.erase(0, BytesToReport);
			}
			return;
		}
		if (
			(LineEnd - idxBoundary != mBoundary.size()) &&  // Line length not equal to boundary
			(LineEnd - idxBoundary != mBoundary.size() + 2)  // Line length not equal to boundary end
		)
		{
			// Got a line, but it's not a boundary, report it as data:
			mCallbacks.onPartData(mIncomingData.data(), LineEnd);
			mIncomingData.erase(0, LineEnd);
			continue;
		}

		if (strncmp(mIncomingData.c_str() + idxBoundary, mBoundary.c_str(), mBoundary.size()) == 0)
		{
			// Boundary or BoundaryEnd found:
			mCallbacks.onPartEnd();
			size_t idxSlash = idxBoundary + mBoundary.size();
			if ((mIncomingData[idxSlash] == '-') && (mIncomingData[idxSlash + 1] == '-'))
			{
				// This was the last part
				mCallbacks.onPartData(mIncomingData.data() + idxSlash + 4, mIncomingData.size() - idxSlash - 4);
				mIncomingData.clear();
				return;
			}
			mCallbacks.onPartStart();
			mIncomingData.erase(0, LineEnd + 2);

			// Keep parsing for the headers that may have come with this data:
			mEnvelopeParser.reset();
			continue;
		}

		// It's a line, but not a boundary. It can be fully sent to the data receiver, since a boundary cannot cross lines
		mCallbacks.onPartData(mIncomingData.c_str(), LineEnd);
		mIncomingData.erase(0, LineEnd);
	}  // while (true)
}





void MultipartParser::onHeaderLine(const std::string & aKey, const std::string & aValue)
{
	mCallbacks.onPartHeader(aKey, aValue);
}





}  // namespace Http
