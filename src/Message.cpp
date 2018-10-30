#include "Message.hpp"
#include <cstring>





namespace Http {





////////////////////////////////////////////////////////////////////////////////
// Message:

Message::Message(Kind aKind) :
	mKind(aKind),
	mContentLength(std::string::npos)
{
}





void Message::addHeader(const std::string & aKey, const std::string & aValue)
{
	auto Key = Utils::strToLower(aKey);
	auto itr = mHeaders.find(Key);
	if (itr == mHeaders.end())
	{
		mHeaders[Key] = aValue;
	}
	else
	{
		// The header-field key is specified multiple times, combine into comma-separated list (RFC 2616 @ 4.2)
		itr->second.append(", ");
		itr->second.append(aValue);
	}

	// Special processing for well-known headers:
	if (Key == "content-type")
	{
		mContentType = mHeaders[Key];
	}
	else if (Key == "content-length")
	{
		if (!Utils::stringToInteger(mHeaders[Key], mContentLength))
		{
			mContentLength = 0;
		}
	}
}





std::string Message::headerToValue(const std::string & aKey, const std::string & aDefault) const
{
	auto itr = mHeaders.find(Utils::strToLower(aKey));
	if (itr == mHeaders.cend())
	{
		return aDefault;
	}
	return itr->second;
}





void Message::setContentType(const std::string & aContentType)
{
	mHeaders["content-type"] = aContentType;
	mContentType = aContentType;
}





void Message::setContentLength(size_t aContentLength)
{
	mHeaders["content-length"] = Utils::printf("%llu", (static_cast<unsigned long long>(aContentLength)));
	mContentLength = aContentLength;
}





////////////////////////////////////////////////////////////////////////////////
// OutgoingResponse:

OutgoingResponse::OutgoingResponse() :
	Super(mkResponse)
{
}





std::string OutgoingResponse::serialize(int aStatusCode, const std::string & aStatusText) const
{
	// Status line:
	auto res = Utils::printf("HTTP/1.1 %d %s\r\n",
		aStatusCode, aStatusText.c_str()
	);

	// Headers:
	for (const auto & hdr: mHeaders)
	{
		res.append(hdr.first);
		res.append(": ");
		res.append(hdr.second);
		res.append("\r\n");
	}  // for itr - mHeaders[]
	res.append("\r\n");

	return res;
}





////////////////////////////////////////////////////////////////////////////////
// SimpleOutgoingResponse:

std::string SimpleOutgoingResponse::serialize(
	int aStatusCode,
	const std::string & aStatusText,
	const std::string & aBody
)
{
	return serialize(
		aStatusCode,
		aStatusText,
		{ {"Content-Length", Utils::printf("%llu", aBody.size())} },
		aBody
	);
}





std::string SimpleOutgoingResponse::serialize(
	int aStatusCode,
	const std::string & aStatusText,
	const std::string & aContentType,
	const std::string & aBody
)
{
	std::map<std::string, std::string> headers =
	{
		{ "Content-Type", aContentType },
		{ "Content-Length", Utils::printf("%llu", aBody.size()) },
	};
	return serialize(
		aStatusCode,
		aStatusText,
		headers,
		aBody
	);
}





std::string SimpleOutgoingResponse::serialize(
	int aStatusCode,
	const std::string & aStatusText,
	const std::map<std::string, std::string> & aHeaders,
	const std::string & aBody
)
{
	std::string res = Utils::printf("HTTP/1.1 %d %s\r\n", aStatusCode, aStatusText.c_str());
	for (const auto & hdr: aHeaders)
	{
		res.append(Utils::printf("%s: %s\r\n", hdr.first.c_str(), hdr.second.c_str()));
	}
	res.append("\r\n");
	res.append(aBody);
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// IncomingRequest:

IncomingRequest::IncomingRequest(const std::string & aMethod, const std::string & aURL):
	Super(mkRequest),
	mMethod(aMethod),
	mURL(aURL),
	mHasAuth(false)
{
}





std::string IncomingRequest::urlPath() const
{
	auto idxQuestionMark = mURL.find('?');
	if (idxQuestionMark == std::string::npos)
	{
		return mURL;
	}
	else
	{
		return mURL.substr(0, idxQuestionMark);
	}
}





void IncomingRequest::addHeader(const std::string & aKey, const std::string & aValue)
{
	if (
		(Utils::noCaseCompare(aKey, "Authorization") == 0) &&
		(strncmp(aValue.c_str(), "Basic ", 6) == 0)
	)
	{
		std::string UserPass = Utils::base64Decode(aValue.substr(6));
		size_t idxCol = UserPass.find(':');
		if (idxCol != std::string::npos)
		{
			mAuthUsername = UserPass.substr(0, idxCol);
			mAuthPassword = UserPass.substr(idxCol + 1);
			mHasAuth = true;
		}
	}
	if ((aKey == "Connection") && (Utils::noCaseCompare(aValue, "keep-alive") == 0))
	{
		mAllowKeepAlive = true;
	}
	Super::addHeader(aKey, aValue);
}





}  // namespace Http
