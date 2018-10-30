#include "NameValueParser.hpp"
#include <cassert>





namespace Http {





NameValueParser::NameValueParser(bool aAllowsKeyOnly) :
	mState(psKeySpace),
	mAllowsKeyOnly(aAllowsKeyOnly)
{
}





NameValueParser::NameValueParser(const char * aData, size_t aSize, bool aAllowsKeyOnly) :
	mState(psKeySpace),
	mAllowsKeyOnly(aAllowsKeyOnly)
{
	parse(aData, aSize);
}





void NameValueParser::parse(const char * aData, size_t aSize)
{
	assert(mState != psFinished);  // Calling Parse() after Finish() is wrong!

	size_t Last = 0;
	for (size_t i = 0; i < aSize;)
	{
		switch (mState)
		{
			case psInvalid:
			case psFinished:
			{
				return;
			}

			case psKeySpace:
			{
				// Skip whitespace until a non-whitespace is found, then start the key:
				while ((i < aSize) && (aData[i] <= ' '))
				{
					i++;
				}
				if ((i < aSize) && (aData[i] > ' '))
				{
					mState = psKey;
					Last = i;
				}
				break;
			}

			case psKey:
			{
				// Read the key until whitespace or an equal sign:
				while (i < aSize)
				{
					if (aData[i] == '=')
					{
						mCurrentKey.append(aData + Last, i - Last);
						i++;
						Last = i;
						mState = psEqual;
						break;
					}
					else if (aData[i] <= ' ')
					{
						mCurrentKey.append(aData + Last, i - Last);
						i++;
						Last = i;
						mState = psEqualSpace;
						break;
					}
					else if (aData[i] == ';')
					{
						if (!mAllowsKeyOnly)
						{
							mState = psInvalid;
							return;
						}
						mCurrentKey.append(aData + Last, i - Last);
						i++;
						Last = i;
						(*this)[mCurrentKey] = "";
						mCurrentKey.clear();
						mState = psKeySpace;
						break;
					}
					else if ((aData[i] == '\"') || (aData[i] == '\''))
					{
						mState = psInvalid;
						return;
					}
					i++;
				}  // while (i < aSize)
				if (i == aSize)
				{
					// Still the key, ran out of data to parse, store the part of the key parsed so far:
					mCurrentKey.append(aData + Last, aSize - Last);
					return;
				}
				break;
			}

			case psEqualSpace:
			{
				// The space before the expected equal sign; the current key is already assigned
				while (i < aSize)
				{
					if (aData[i] == '=')
					{
						mState = psEqual;
						i++;
						Last = i;
						break;
					}
					else if (aData[i] == ';')
					{
						// Key-only
						if (!mAllowsKeyOnly)
						{
							mState = psInvalid;
							return;
						}
						i++;
						Last = i;
						(*this)[mCurrentKey] = "";
						mCurrentKey.clear();
						mState = psKeySpace;
						break;
					}
					else if (aData[i] > ' ')
					{
						mState = psInvalid;
						return;
					}
					i++;
				}  // while (i < aSize)
				break;
			}  // case psEqualSpace

			case psEqual:
			{
				// just parsed the equal-sign
				while (i < aSize)
				{
					if (aData[i] == ';')
					{
						if (!mAllowsKeyOnly)
						{
							mState = psInvalid;
							return;
						}
						i++;
						Last = i;
						(*this)[mCurrentKey] = "";
						mCurrentKey.clear();
						mState = psKeySpace;
						break;
					}
					else if (aData[i] == '\"')
					{
						i++;
						Last = i;
						mState = psValueInDQuotes;
						break;
					}
					else if (aData[i] == '\'')
					{
						i++;
						Last = i;
						mState = psValueInSQuotes;
						break;
					}
					else
					{
						mCurrentValue.push_back(aData[i]);
						i++;
						Last = i;
						mState = psValueRaw;
						break;
					}
				}  // while (i < aSize)
				break;
			}  // case psEqual

			case psValueInDQuotes:
			{
				while (i < aSize)
				{
					if (aData[i] == '\"')
					{
						mCurrentValue.append(aData + Last, i - Last);
						(*this)[mCurrentKey] = mCurrentValue;
						mCurrentKey.clear();
						mCurrentValue.clear();
						mState = psAfterValue;
						i++;
						Last = i;
						break;
					}
					i++;
				}  // while (i < aSize)
				if (i == aSize)
				{
					mCurrentValue.append(aData + Last, aSize - Last);
				}
				break;
			}  // case psValueInDQuotes

			case psValueInSQuotes:
			{
				while (i < aSize)
				{
					if (aData[i] == '\'')
					{
						mCurrentValue.append(aData + Last, i - Last);
						(*this)[mCurrentKey] = mCurrentValue;
						mCurrentKey.clear();
						mCurrentValue.clear();
						mState = psAfterValue;
						i++;
						Last = i;
						break;
					}
					i++;
				}  // while (i < aSize)
				if (i == aSize)
				{
					mCurrentValue.append(aData + Last, aSize - Last);
				}
				break;
			}  // case psValueInSQuotes

			case psValueRaw:
			{
				while (i < aSize)
				{
					if (aData[i] == ';')
					{
						mCurrentValue.append(aData + Last, i - Last);
						(*this)[mCurrentKey] = mCurrentValue;
						mCurrentKey.clear();
						mCurrentValue.clear();
						mState = psKeySpace;
						i++;
						Last = i;
						break;
					}
					i++;
				}
				if (i == aSize)
				{
					mCurrentValue.append(aData + Last, aSize - Last);
				}
				break;
			}  // case psValueRaw

			case psAfterValue:
			{
				// Between the closing DQuote or SQuote and the terminating semicolon
				while (i < aSize)
				{
					if (aData[i] == ';')
					{
						mState = psKeySpace;
						i++;
						Last = i;
						break;
					}
					else if (aData[i] < ' ')
					{
						i++;
						continue;
					}
					mState = psInvalid;
					return;
				}  // while (i < aSize)
				break;
			}
		}  // switch (mState)
	}  // for i - aData[]
}





bool NameValueParser::finish()
{
	switch (mState)
	{
		case psInvalid:
		{
			return false;
		}
		case psFinished:
		{
			return true;
		}
		case psKey:
		case psEqualSpace:
		case psEqual:
		{
			if ((mAllowsKeyOnly) && !mCurrentKey.empty())
			{
				(*this)[mCurrentKey] = "";
				mState = psFinished;
				return true;
			}
			mState = psInvalid;
			return false;
		}
		case psValueRaw:
		{
			(*this)[mCurrentKey] = mCurrentValue;
			mState = psFinished;
			return true;
		}
		case psValueInDQuotes:
		case psValueInSQuotes:
		{
			// Missing the terminating quotes, this is an error
			mState = psInvalid;
			return false;
		}
		case psKeySpace:
		case psAfterValue:
		{
			mState = psFinished;
			return true;
		}
	}
	assert(!"Unhandled parser state!");
	#ifndef __clang__
		return false;
	#endif
}





}  // namespace Http
