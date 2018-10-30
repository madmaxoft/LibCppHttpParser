#include "FormParser.hpp"
#include <vector>
#include <cassert>
#include <cstring>
#include "Utils.hpp"
#include "Message.hpp"
#include "MultipartParser.hpp"
#include "NameValueParser.hpp"





namespace Http {





FormParser::FormParser(const IncomingRequest & aRequest, Callbacks & aCallbacks) :
	mCallbacks(aCallbacks),
	mIsValid(true),
	mIsCurrentPartFile(false),
	mFileHasBeenAnnounced(false)
{
	if (aRequest.method() == "GET")
	{
		mKind = fpkURL;

		// Directly parse the URL in the request:
		const std::string & URL = aRequest.url();
		size_t idxQM = URL.find('?');
		if (idxQM != std::string::npos)
		{
			parse(URL.c_str() + idxQM + 1, URL.size() - idxQM - 1);
		}
		return;
	}
	if ((aRequest.method() == "POST") || (aRequest.method() == "PUT"))
	{
		if (strncmp(aRequest.contentType().c_str(), "application/x-www-form-urlencoded", 33) == 0)
		{
			mKind = fpkFormUrlEncoded;
			return;
		}
		if (strncmp(aRequest.contentType().c_str(), "multipart/form-data", 19) == 0)
		{
			mKind = fpkMultipart;
			beginMultipart(aRequest);
			return;
		}
	}
	// Invalid method / content type combination, this is not a HTTP form
	mIsValid = false;
}





FormParser::FormParser(Kind aKind, const char * aData, size_t aSize, Callbacks & aCallbacks) :
	mCallbacks(aCallbacks),
	mKind(aKind),
	mIsValid(true),
	mIsCurrentPartFile(false),
	mFileHasBeenAnnounced(false)
{
	parse(aData, aSize);
}





void FormParser::parse(const char * aData, size_t aSize)
{
	if (!mIsValid)
	{
		return;
	}

	switch (mKind)
	{
		case fpkURL:
		case fpkFormUrlEncoded:
		{
			// This format is used for smaller forms (not file uploads), so we can delay parsing it until Finish()
			mIncomingData.append(aData, aSize);
			break;
		}
		case fpkMultipart:
		{
			assert(mMultipartParser.get() != nullptr);
			mMultipartParser->parse(aData, aSize);
			break;
		}
	}
}





bool FormParser::finish()
{
	switch (mKind)
	{
		case fpkURL:
		case fpkFormUrlEncoded:
		{
			// mIncomingData has all the form data, parse it now:
			parseFormUrlEncoded();
			break;
		}
		case fpkMultipart:
		{
			// Nothing needed for other formats
			break;
		}
	}
	return (mIsValid && mIncomingData.empty());
}





bool FormParser::hasFormData(const IncomingRequest & aRequest)
{
	const std::string & ContentType = aRequest.contentType();
	return (
		(ContentType == "application/x-www-form-urlencoded") ||
		(strncmp(ContentType.c_str(), "multipart/form-data", 19) == 0) ||
		(
			(aRequest.method() == "GET") &&
			(aRequest.url().find('?') != std::string::npos)
		)
	);
}





void FormParser::beginMultipart(const IncomingRequest & aRequest)
{
	assert(mMultipartParser.get() == nullptr);
	mMultipartParser.reset(new MultipartParser(aRequest.contentType(), *this));
}





void FormParser::parseFormUrlEncoded()
{
	// Parse mIncomingData for all the variables; no more data is incoming, since this is called from Finish()
	// This may not be the most performant version, but we don't care, the form data is small enough and we're not a full-fledged web server anyway
	auto lines = Utils::stringSplit(mIncomingData, "&");
	for (auto itr = lines.begin(), end = lines.end(); itr != end; ++itr)
	{
		auto Components = Utils::stringSplit(*itr, "=");
		switch (Components.size())
		{
			default:
			{
				// Neither name nor value, or too many "="s, mark this as invalid form:
				mIsValid = false;
				return;
			}
			case 1:
			{
				// Only name present
				auto name = Utils::urlDecode(Utils::replaceAllCharOccurrences(Components[0], '+', ' '));
				if (name.first)
				{
					(*this)[name.second] = "";
				}
				break;
			}
			case 2:
			{
				// name=value format:
				auto name = Utils::urlDecode(Components[0]);
				auto value = Utils::urlDecode(Components[1]);
				if (name.first && value.first)
				{
					(*this)[name.second] = value.second;
				}
				break;
			}
		}
	}  // for itr - Lines[]
	mIncomingData.clear();
}





void FormParser::onPartStart()
{
	mCurrentPartFileName.clear();
	mCurrentPartName.clear();
	mIsCurrentPartFile = false;
	mFileHasBeenAnnounced = false;
}





void FormParser::onPartHeader(const std::string & aKey, const std::string & aValue)
{
	if (Utils::noCaseCompare(aKey, "Content-Disposition") == 0)
	{
		size_t len = aValue.size();
		size_t ParamsStart = std::string::npos;
		for (size_t i = 0; i < len; ++i)
		{
			if (aValue[i] > ' ')
			{
				if (strncmp(aValue.c_str() + i, "form-data", 9) != 0)
				{
					// Content disposition is not "form-data", mark the whole form invalid
					mIsValid = false;
					return;
				}
				ParamsStart = aValue.find(';', i + 9);
				break;
			}
		}
		if (ParamsStart == std::string::npos)
		{
			// There is data missing in the Content-Disposition field, mark the whole form invalid:
			mIsValid = false;
			return;
		}

		// Parse the field name and optional filename from this header:
		NameValueParser Parser(aValue.data() + ParamsStart, aValue.size() - ParamsStart);
		Parser.finish();
		mCurrentPartName = Parser["name"];
		if (!Parser.isValid() || mCurrentPartName.empty())
		{
			// The required parameter "name" is missing, mark the whole form invalid:
			mIsValid = false;
			return;
		}
		mCurrentPartFileName = Parser["filename"];
	}
}





void FormParser::onPartData(const char * aData, size_t aSize)
{
	if (mCurrentPartName.empty())
	{
		// Prologue, epilogue or invalid part
		return;
	}
	if (mCurrentPartFileName.empty())
	{
		// This is a variable, store it in the map
		iterator itr = find(mCurrentPartName);
		if (itr == end())
		{
			(*this)[mCurrentPartName] = std::string(aData, aSize);
		}
		else
		{
			itr->second.append(aData, aSize);
		}
	}
	else
	{
		// This is a file, pass it on through the callbacks
		if (!mFileHasBeenAnnounced)
		{
			mCallbacks.onFileStart(*this, mCurrentPartFileName);
			mFileHasBeenAnnounced = true;
		}
		mCallbacks.onFileData(*this, aData, aSize);
	}
}





void FormParser::onPartEnd()
{
	if (mFileHasBeenAnnounced)
	{
		mCallbacks.onFileEnd(*this);
	}
	mCurrentPartName.clear();
	mCurrentPartFileName.clear();
}




}  // namespace Http
