#include "TransferEncodingParser.hpp"
#include <cassert>
#include <algorithm>
#include "EnvelopeParser.hpp"
#include "Utils.hpp"





namespace Http {





////////////////////////////////////////////////////////////////////////////////
// ChunkedTEParser:

class ChunkedTEParser:
	public TransferEncodingParser,
	public EnvelopeParser::Callbacks
{
	typedef TransferEncodingParser Super;

public:
	ChunkedTEParser(Super::Callbacks & aCallbacks):
		Super(aCallbacks),
		mState(psChunkLength),
		mChunkDataLengthLeft(0),
		mTrailerParser(*this)
	{
	}


protected:
	enum eState
	{
		psChunkLength,         ///< Parsing the chunk length hex number
		psChunkLengthTrailer,  ///< Any trailer (chunk extension) specified after the chunk length
		psChunkLengthLF,       ///< The LF character after the CR character terminating the chunk length
		psChunkData,           ///< Relaying chunk data
		psChunkDataCR,         ///< Skipping the extra CR character after chunk data
		psChunkDataLF,         ///< Skipping the extra LF character after chunk data
		psTrailer,             ///< Received an empty chunk, parsing the trailer (through the envelope parser)
		psFinished,            ///< The parser has finished parsing, either successfully or with an error
	};

	/** The current state of the parser (parsing chunk length / chunk data). */
	eState mState;

	/** Number of bytes that still belong to the chunk currently being parsed.
	When in psChunkLength, the value is the currently parsed length digits. */
	size_t mChunkDataLengthLeft;

	/** The parser used for the last (empty) chunk's trailer data */
	EnvelopeParser mTrailerParser;


	/** Calls the onError callback and sets parser state to finished. */
	void error(const std::string & aErrorMsg)
	{
		mState = psFinished;
		mCallbacks.onError(aErrorMsg);
	}


	/** Parses the incoming data, the current state is psChunkLength.
	Stops parsing when either the chunk length has been read, or there is no more data in the input.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLength(const char * aData, size_t aSize)
	{
		// Expected input: <hexnumber>[;<trailer>]<CR><LF>
		// Only the hexnumber is parsed into mChunkDataLengthLeft, the rest is postponed into psChunkLengthTrailer or psChunkLengthLF
		for (size_t i = 0; i < aSize; i++)
		{
			switch (aData[i])
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					mChunkDataLengthLeft = mChunkDataLengthLeft * 16 + static_cast<decltype(mChunkDataLengthLeft)>(aData[i] - '0');
					break;
				}
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				{
					mChunkDataLengthLeft = mChunkDataLengthLeft * 16 + static_cast<decltype(mChunkDataLengthLeft)>(aData[i] - 'a' + 10);
					break;
				}
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				{
					mChunkDataLengthLeft = mChunkDataLengthLeft * 16 + static_cast<decltype(mChunkDataLengthLeft)>(aData[i] - 'A' + 10);
					break;
				}
				case '\r':
				{
					mState = psChunkLengthLF;
					return i + 1;
				}
				case ';':
				{
					mState = psChunkLengthTrailer;
					return i + 1;
				}
				default:
				{
					error(Utils::printf("Invalid character in chunk length line: 0x%x", aData[i]));
					return std::string::npos;
				}
			}  // switch (aData[i])
		}  // for i - aData[]
		return aSize;
	}


	/** Parses the incoming data, the current state is psChunkLengthTrailer.
	Stops parsing when either the chunk length trailer has been read, or there is no more data in the input.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLengthTrailer(const char * aData, size_t aSize)
	{
		// Expected input: <trailer><CR><LF>
		// The LF itself is not parsed, it is instead postponed into psChunkLengthLF
		for (size_t i = 0; i < aSize; i++)
		{
			switch (aData[i])
			{
				case '\r':
				{
					mState = psChunkLengthLF;
					return i;
				}
				default:
				{
					if (aData[i] < 32)
					{
						// Only printable characters are allowed in the trailer
						error(Utils::printf("Invalid character in chunk length line: 0x%x", aData[i]));
						return std::string::npos;
					}
				}
			}  // switch (aData[i])
		}  // for i - aData[]
		return aSize;
	}


	/** Parses the incoming data, the current state is psChunkLengthLF.
	Only the LF character is expected, if found, moves to psChunkData, otherwise issues an error.
	If the chunk length that just finished reading is equal to 0, signals the end of stream (via psTrailer).
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLengthLF(const char * aData, size_t aSize)
	{
		// Expected input: <LF>
		if (aSize == 0)
		{
			return 0;
		}
		if (aData[0] == '\n')
		{
			if (mChunkDataLengthLeft == 0)
			{
				mState = psTrailer;
			}
			else
			{
				mState = psChunkData;
			}
			return 1;
		}
		error(Utils::printf("Invalid character past chunk length's CR: 0x%x", aData[0]));
		return std::string::npos;
	}


	/** Consumes as much chunk data from the input as possible.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error() handler). */
	size_t parseChunkData(const char * aData, size_t aSize)
	{
		assert(mChunkDataLengthLeft > 0);
		auto bytes = std::min(aSize, mChunkDataLengthLeft);
		mChunkDataLengthLeft -= bytes;
		mCallbacks.onBodyData(aData, bytes);
		if (mChunkDataLengthLeft == 0)
		{
			mState = psChunkDataCR;
		}
		return bytes;
	}


	/** Parses the incoming data, the current state is psChunkDataCR.
	Only the CR character is expected, if found, moves to psChunkDataLF, otherwise issues an error.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkDataCR(const char * aData, size_t aSize)
	{
		// Expected input: <CR>
		if (aSize == 0)
		{
			return 0;
		}
		if (aData[0] == '\r')
		{
			mState = psChunkDataLF;
			return 1;
		}
		error(Utils::printf("Invalid character past chunk data: 0x%x", aData[0]));
		return std::string::npos;
	}




	/** Parses the incoming data, the current state is psChunkDataCR.
	Only the CR character is expected, if found, moves to psChunkDataLF, otherwise issues an error.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkDataLF(const char * aData, size_t aSize)
	{
		// Expected input: <LF>
		if (aSize == 0)
		{
			return 0;
		}
		if (aData[0] == '\n')
		{
			mState = psChunkLength;
			return 1;
		}
		error(Utils::printf("Invalid character past chunk data's CR: 0x%x", aData[0]));
		return std::string::npos;
	}


	/** Parses the incoming data, the current state is psChunkDataCR.
	The trailer is normally a set of "Header: Value" lines, terminated by an empty line. Use the mTrailerParser for that.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseTrailer(const char * aData, size_t aSize)
	{
		auto res = mTrailerParser.parse(aData, aSize);
		if (res == std::string::npos)
		{
			error("Error while parsing the trailer");
		}
		if ((res < aSize) || !mTrailerParser.isInHeaders())
		{
			mCallbacks.onBodyFinished();
			mState = psFinished;
		}
		return res;
	}


	// TransferEncodingParser overrides:
	virtual size_t parse(const char * aData, size_t aSize) override
	{
		while ((aSize > 0) && (mState != psFinished))
		{
			size_t consumed = 0;
			switch (mState)
			{
				case psChunkLength:        consumed = parseChunkLength       (aData, aSize); break;
				case psChunkLengthTrailer: consumed = parseChunkLengthTrailer(aData, aSize); break;
				case psChunkLengthLF:      consumed = parseChunkLengthLF     (aData, aSize); break;
				case psChunkData:          consumed = parseChunkData         (aData, aSize); break;
				case psChunkDataCR:        consumed = parseChunkDataCR       (aData, aSize); break;
				case psChunkDataLF:        consumed = parseChunkDataLF       (aData, aSize); break;
				case psTrailer:            consumed = parseTrailer           (aData, aSize); break;
				case psFinished:           consumed = 0;                                       break;  // Not supposed to happen, but Clang complains without it
			}
			if (consumed == std::string::npos)
			{
				return std::string::npos;
			}
			aData += consumed;
			aSize -= consumed;
		}
		return aSize;
	}

	virtual void finish() override
	{
		if (mState != psFinished)
		{
			error(Utils::printf("ChunkedTransferEncoding: Finish signal received before the data stream ended (state: %d)", mState));
		}
		mState = psFinished;
	}


	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & /* aKey */, const std::string & /* aValue */) override
	{
		// Ignored
	}
};





////////////////////////////////////////////////////////////////////////////////
// IdentityTEParser:

class IdentityTEParser:
	public TransferEncodingParser
{
	typedef TransferEncodingParser Super;

public:
	IdentityTEParser(Callbacks & aCallbacks, size_t aContentLength):
		Super(aCallbacks),
		mBytesLeft(aContentLength)
	{
	}


protected:

	/** How many bytes of content are left before the message ends. */
	size_t mBytesLeft;

	// TransferEncodingParser overrides:
	virtual size_t parse(const char * aData, size_t aSize) override
	{
		auto size = std::min(aSize, mBytesLeft);
		if (size > 0)
		{
			mCallbacks.onBodyData(aData, size);
		}
		mBytesLeft -= size;
		if (mBytesLeft == 0)
		{
			mCallbacks.onBodyFinished();
		}
		return aSize - size;
	}

	virtual void finish() override
	{
		if (mBytesLeft > 0)
		{
			mCallbacks.onError("IdentityTransferEncoding: body was truncated");
		}
		else
		{
			// BodyFinished has already been called, just bail out
		}
	}
};





////////////////////////////////////////////////////////////////////////////////
// TransferEncodingParser:

TransferEncodingParserPtr TransferEncodingParser::create(
	Callbacks & aCallbacks,
	const std::string & aTransferEncoding,
	size_t aContentLength
)
{
	auto transferEncoding = Utils::strToLower(aTransferEncoding);
	if (transferEncoding == "chunked")
	{
		return std::make_shared<ChunkedTEParser>(aCallbacks);
	}
	if (transferEncoding == "identity")
	{
		return std::make_shared<IdentityTEParser>(aCallbacks, aContentLength);
	}
	return nullptr;
}





}  // namespace Http
