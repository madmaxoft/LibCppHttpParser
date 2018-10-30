#include "EnvelopeParser.hpp"
#include <cassert>





namespace Http {





EnvelopeParser::EnvelopeParser(Callbacks & aCallbacks) :
	mCallbacks(aCallbacks),
	mIsInHeaders(true)
{
}





size_t EnvelopeParser::parse(const char * aData, size_t aSize)
{
	if (!mIsInHeaders)
	{
		return 0;
	}

	// Start searching 1 char from the end of the already received data, if available:
	auto searchStart = mIncomingData.size();
	searchStart = (searchStart > 1) ? searchStart - 1 : 0;

	mIncomingData.append(aData, aSize);

	size_t idxCRLF = mIncomingData.find("\r\n", searchStart);
	if (idxCRLF == std::string::npos)
	{
		// Not a complete line yet, all input consumed:
		return aSize;
	}

	// Parse as many lines as found:
	size_t last = 0;
	do
	{
		if (idxCRLF == last)
		{
			// This was the last line of the data. Finish whatever value has been cached and return:
			notifyLast();
			mIsInHeaders = false;
			return aSize - (mIncomingData.size() - idxCRLF) + 2;
		}
		if (!parseLine(mIncomingData.c_str() + last, idxCRLF - last))
		{
			// An error has occurred
			mIsInHeaders = false;
			return std::string::npos;
		}
		last = idxCRLF + 2;
		idxCRLF = mIncomingData.find("\r\n", idxCRLF + 2);
	} while (idxCRLF != std::string::npos);
	mIncomingData.erase(0, last);

	// Parsed all lines and still expecting more
	return aSize;
}





void EnvelopeParser::reset()
{
	mIsInHeaders = true;
	mIncomingData.clear();
	mLastKey.clear();
	mLastValue.clear();
}





void EnvelopeParser::notifyLast()
{
	if (!mLastKey.empty())
	{
		mCallbacks.onHeaderLine(mLastKey, mLastValue);
		mLastKey.clear();
	}
	mLastValue.clear();
}





bool EnvelopeParser::parseLine(const char * aData, size_t aSize)
{
	assert(aSize > 0);
	if (aData[0] <= ' ')
	{
		// This line is a continuation for the previous line
		if (mLastKey.empty())
		{
			return false;
		}
		// Append, including the whitespace in aData[0]
		mLastValue.append(aData, aSize);
		return true;
	}

	// This is a line with a new key:
	notifyLast();
	for (size_t i = 0; i < aSize; i++)
	{
		if (aData[i] == ':')
		{
			mLastKey.assign(aData, i);
			if (aSize > i + 1)
			{
				mLastValue.assign(aData + i + 2, aSize - i - 2);
			}
			else
			{
				mLastValue.clear();
			}
			return true;
		}
	}  // for i - aData[]

	// No colon was found, key-less header??
	return false;
}




}  // namespace Http
