#include "MessageParser.hpp"
#include <cassert>
#include "Utils.hpp"





namespace Http {





MessageParser::MessageParser(MessageParser::Callbacks & aCallbacks):
	mCallbacks(aCallbacks),
	mEnvelopeParser(*this)
{
	reset();
}





size_t MessageParser::parse(const char * aData, size_t aSize)
{
	// If parsing already finished or errorred, let the caller keep all the data:
	if (mIsFinished || mHasHadError)
	{
		return 0;
	}

	// If still waiting for the status line, add to buffer and try parsing it:
	auto inBufferSoFar = mBuffer.size();
	if (mFirstLine.empty())
	{
		mBuffer.append(aData, aSize);
		auto bytesConsumedFirstLine = parseFirstLine();
		assert(bytesConsumedFirstLine <= inBufferSoFar + aSize);  // Haven't consumed more data than there is in the buffer
		assert(bytesConsumedFirstLine > inBufferSoFar);  // Have consumed at least the previous buffer contents
		if (mFirstLine.empty())
		{
			// All data used, but not a complete status line yet.
			return aSize;
		}
		if (mHasHadError)
		{
			return std::string::npos;
		}
		// Status line completed, feed the rest of the buffer into the envelope parser:
		auto bytesConsumedEnvelope = mEnvelopeParser.parse(mBuffer.data(), mBuffer.size());
		if (bytesConsumedEnvelope == std::string::npos)
		{
			mHasHadError = true;
			mCallbacks.onError("Failed to parse the envelope");
			return std::string::npos;
		}
		assert(bytesConsumedEnvelope <= bytesConsumedFirstLine + aSize);  // Haven't consumed more data than there was in the buffer
		mBuffer.erase(0, bytesConsumedEnvelope);
		if (!mEnvelopeParser.isInHeaders())
		{
			headersFinished();
			// Process any data still left in the buffer as message body:
			auto bytesConsumedBody = parseBody(mBuffer.data(), mBuffer.size());
			if (bytesConsumedBody == std::string::npos)
			{
				// Error has already been reported by ParseBody, just bail out:
				return std::string::npos;
			}
			return bytesConsumedBody + bytesConsumedEnvelope + bytesConsumedFirstLine - inBufferSoFar;
		}
		return aSize;
	}  // if (mFirstLine.empty())

	// If still parsing headers, send them to the envelope parser:
	if (mEnvelopeParser.isInHeaders())
	{
		auto bytesConsumed = mEnvelopeParser.parse(aData, aSize);
		if (bytesConsumed == std::string::npos)
		{
			mHasHadError = true;
			mCallbacks.onError("Failed to parse the envelope");
			return std::string::npos;
		}
		if (!mEnvelopeParser.isInHeaders())
		{
			headersFinished();
			// Process any data still left as message body:
			auto bytesConsumedBody = parseBody(aData + bytesConsumed, aSize - bytesConsumed);
			if (bytesConsumedBody == std::string::npos)
			{
				// Error has already been reported by ParseBody, just bail out:
				return std::string::npos;
			}
		}
		return aSize;
	}

	// Already parsing the body
	return parseBody(aData, aSize);
}





void MessageParser::reset()
{
	mHasHadError = false;
	mIsFinished = false;
	mFirstLine.clear();
	mBuffer.clear();
	mEnvelopeParser.reset();
	mTransferEncodingParser.reset();
	mTransferEncoding.clear();
	mContentLength = 0;
}




size_t MessageParser::parseFirstLine()
{
	auto idxLineEnd = mBuffer.find("\r\n");
	if (idxLineEnd == std::string::npos)
	{
		// Not a complete line yet
		return mBuffer.size();
	}
	mFirstLine = mBuffer.substr(0, idxLineEnd);
	mBuffer.erase(0, idxLineEnd + 2);
	mCallbacks.onFirstLine(mFirstLine);
	return idxLineEnd + 2;
}





size_t MessageParser::parseBody(const char * aData, size_t aSize)
{
	if (mTransferEncodingParser == nullptr)
	{
		// We have no Transfer-encoding parser assigned. This should have happened when finishing the envelope
		onError("No transfer encoding parser");
		return std::string::npos;
	}

	// Parse the body using the transfer encoding parser:
	// (Note that TE parser returns the number of bytes left, while we return the number of bytes consumed)
	return aSize - mTransferEncodingParser->parse(aData, aSize);
}





void MessageParser::headersFinished()
{
	mCallbacks.onHeadersFinished();
	if (mTransferEncoding.empty())
	{
		mTransferEncoding = "Identity";
	}
	mTransferEncodingParser = TransferEncodingParser::create(*this, mTransferEncoding, mContentLength);
	if (mTransferEncodingParser == nullptr)
	{
		onError(Utils::printf("Unknown transfer encoding: %s", mTransferEncoding.c_str()));
		return;
	}
}





void MessageParser::onHeaderLine(const std::string & aKey, const std::string & aValue)
{
	mCallbacks.onHeaderLine(aKey, aValue);
	auto key = Utils::strToLower(aKey);
	if (key == "content-length")
	{
		if (!Utils::stringToInteger(aValue, mContentLength))
		{
			onError(Utils::printf("Invalid content length header value: \"%s\"", aValue.c_str()));
		}
		return;
	}
	if (key == "transfer-encoding")
	{
		mTransferEncoding = aValue;
		return;
	}
}





void MessageParser::onError(const std::string & aErrorDescription)
{
	mHasHadError = true;
	mCallbacks.onError(aErrorDescription);
}





void MessageParser::onBodyData(const void * aData, size_t aSize)
{
	mCallbacks.onBodyData(aData, aSize);
}





void MessageParser::onBodyFinished()
{
	mIsFinished = true;
	mCallbacks.onBodyFinished();
}





}  // namespace Http
