// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_content_disposition.h"

#include "base/base64.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace {

enum RFC2047EncodingType {
  Q_ENCODING,
  B_ENCODING
};

// Decodes a "Q" encoded string as described in RFC 2047 section 4.2. Similar to
// decoding a quoted-printable string.  Returns true if the input was valid.
bool DecodeQEncoding(const std::string& input, std::string* output) {
  std::string temp;
  temp.reserve(input.size());
  for (std::string::const_iterator it = input.begin(); it != input.end();
       ++it) {
    if (*it == '_') {
      temp.push_back(' ');
    } else if (*it == '=') {
      if ((input.end() - it < 3) ||
          !IsHexDigit(static_cast<unsigned char>(*(it + 1))) ||
          !IsHexDigit(static_cast<unsigned char>(*(it + 2))))
        return false;
      unsigned char ch = HexDigitToInt(*(it + 1)) * 16 +
                         HexDigitToInt(*(it + 2));
      temp.push_back(static_cast<char>(ch));
      ++it;
      ++it;
    } else if (0x20 < *it && *it < 0x7F && *it != '?') {
      // In a Q-encoded word, only printable ASCII characters
      // represent themselves. Besides, space, '=', '_' and '?' are
      // not allowed, but they're already filtered out.
      DCHECK_NE('=', *it);
      DCHECK_NE('?', *it);
      DCHECK_NE('_', *it);
      temp.push_back(*it);
    } else {
      return false;
    }
  }
  output->swap(temp);
  return true;
}

// Decodes a "Q" or "B" encoded string as per RFC 2047 section 4. The encoding
// type is specified in |enc_type|.
bool DecodeBQEncoding(const std::string& part,
                      RFC2047EncodingType enc_type,
                      const std::string& charset,
                      std::string* output) {
  std::string decoded;
  if (!((enc_type == B_ENCODING) ?
        base::Base64Decode(part, &decoded) : DecodeQEncoding(part, &decoded)))
    return false;

  if (decoded.empty()) {
    output->clear();
    return true;
  }

  UErrorCode err = U_ZERO_ERROR;
  UConverter* converter(ucnv_open(charset.c_str(), &err));
  if (U_FAILURE(err))
    return false;

  // A single byte in a legacy encoding can be expanded to 3 bytes in UTF-8.
  // A 'two-byte character' in a legacy encoding can be expanded to 4 bytes
  // in UTF-8. Therefore, the expansion ratio is 3 at most. Add one for a
  // trailing '\0'.
  size_t output_length = decoded.length() * 3 + 1;
  char* buf = WriteInto(output, output_length);
  output_length = ucnv_toAlgorithmic(UCNV_UTF8, converter, buf, output_length,
                                     decoded.data(), decoded.length(), &err);
  ucnv_close(converter);
  if (U_FAILURE(err))
    return false;
  output->resize(output_length);
  return true;
}

bool DecodeWord(const std::string& encoded_word,
                const std::string& referrer_charset,
                bool* is_rfc2047,
                std::string* output,
                int* parse_result_flags) {
  *is_rfc2047 = false;
  output->clear();
  if (encoded_word.empty())
    return true;

  if (!IsStringASCII(encoded_word)) {
    // Try UTF-8, referrer_charset and the native OS default charset in turn.
    if (IsStringUTF8(encoded_word)) {
      *output = encoded_word;
    } else {
      base::string16 utf16_output;
      if (!referrer_charset.empty() &&
          base::CodepageToUTF16(encoded_word, referrer_charset.c_str(),
                                base::OnStringConversionError::FAIL,
                                &utf16_output)) {
        *output = base::UTF16ToUTF8(utf16_output);
      } else {
        *output = base::WideToUTF8(base::SysNativeMBToWide(encoded_word));
      }
    }

    *parse_result_flags |= net::HttpContentDisposition::HAS_NON_ASCII_STRINGS;
    return true;
  }

  // RFC 2047 : one of encoding methods supported by Firefox and relatively
  // widely used by web servers.
  // =?charset?<E>?<encoded string>?= where '<E>' is either 'B' or 'Q'.
  // We don't care about the length restriction (72 bytes) because
  // many web servers generate encoded words longer than the limit.
  std::string decoded_word;
  *is_rfc2047 = true;
  int part_index = 0;
  std::string charset;
  base::StringTokenizer t(encoded_word, "?");
  RFC2047EncodingType enc_type = Q_ENCODING;
  while (*is_rfc2047 && t.GetNext()) {
    std::string part = t.token();
    switch (part_index) {
      case 0:
        if (part != "=") {
          *is_rfc2047 = false;
          break;
        }
        ++part_index;
        break;
      case 1:
        // Do we need charset validity check here?
        charset = part;
        ++part_index;
        break;
      case 2:
        if (part.size() > 1 ||
            part.find_first_of("bBqQ") == std::string::npos) {
          *is_rfc2047 = false;
          break;
        }
        if (part[0] == 'b' || part[0] == 'B') {
          enc_type = B_ENCODING;
        }
        ++part_index;
        break;
      case 3:
        *is_rfc2047 = DecodeBQEncoding(part, enc_type, charset, &decoded_word);
        if (!*is_rfc2047) {
          // Last minute failure. Invalid B/Q encoding. Rather than
          // passing it through, return now.
          return false;
        }
        ++part_index;
        break;
      case 4:
        if (part != "=") {
          // Another last minute failure !
          // Likely to be a case of two encoded-words in a row or
          // an encoded word followed by a non-encoded word. We can be
          // generous, but it does not help much in terms of compatibility,
          // I believe. Return immediately.
          *is_rfc2047 = false;
          return false;
        }
        ++part_index;
        break;
      default:
        *is_rfc2047 = false;
        return false;
    }
  }

  if (*is_rfc2047) {
    if (*(encoded_word.end() - 1) == '=') {
      output->swap(decoded_word);
      *parse_result_flags |=
          net::HttpContentDisposition::HAS_RFC2047_ENCODED_STRINGS;
      return true;
    }
    // encoded_word ending prematurelly with '?' or extra '?'
    *is_rfc2047 = false;
    return false;
  }

  // We're not handling 'especial' characters quoted with '\', but
  // it should be Ok because we're not an email client but a
  // web browser.

  // What IE6/7 does: %-escaped UTF-8.
  decoded_word = net::UnescapeURLComponent(encoded_word,
                                           net::UnescapeRule::SPACES);
  if (decoded_word != encoded_word)
    *parse_result_flags |=
        net::HttpContentDisposition::HAS_PERCENT_ENCODED_STRINGS;
  if (IsStringUTF8(decoded_word)) {
    output->swap(decoded_word);
    return true;
    // We can try either the OS default charset or 'origin charset' here,
    // As far as I can tell, IE does not support it. However, I've seen
    // web servers emit %-escaped string in a legacy encoding (usually
    // origin charset).
    // TODO(jungshik) : Test IE further and consider adding a fallback here.
  }
  return false;
}

// Decodes the value of a 'filename' or 'name' parameter given as |input|. The
// value is supposed to be of the form:
//
//   value                   = token | quoted-string
//
// However we currently also allow RFC 2047 encoding and non-ASCII
// strings. Non-ASCII strings are interpreted based on |referrer_charset|.
bool DecodeFilenameValue(const std::string& input,
                         const std::string& referrer_charset,
                         std::string* output,
                         int* parse_result_flags) {
  int current_parse_result_flags = 0;
  std::string decoded_value;
  bool is_previous_token_rfc2047 = true;

  // Tokenize with whitespace characters.
  base::StringTokenizer t(input, " \t\n\r");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      // If the previous non-delimeter token is not RFC2047-encoded,
      // put in a space in its place. Otheriwse, skip over it.
      if (!is_previous_token_rfc2047)
        decoded_value.push_back(' ');
      continue;
    }
    // We don't support a single multibyte character split into
    // adjacent encoded words. Some broken mail clients emit headers
    // with that problem, but most web servers usually encode a filename
    // in a single encoded-word. Firefox/Thunderbird do not support
    // it, either.
    std::string decoded;
    if (!DecodeWord(t.token(), referrer_charset, &is_previous_token_rfc2047,
                    &decoded, &current_parse_result_flags))
      return false;
    decoded_value.append(decoded);
  }
  output->swap(decoded_value);
  if (parse_result_flags && !output->empty())
    *parse_result_flags |= current_parse_result_flags;
  return true;
}

// Parses the charset and value-chars out of an ext-value string.
//
//  ext-value     = charset  "'" [ language ] "'" value-chars
bool ParseExtValueComponents(const std::string& input,
                             std::string* charset,
                             std::string* value_chars) {
  base::StringTokenizer t(input, "'");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  std::string temp_charset;
  std::string temp_value;
  int numDelimsSeen = 0;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      ++numDelimsSeen;
      continue;
    } else {
      switch (numDelimsSeen) {
        case 0:
          temp_charset = t.token();
          break;
        case 1:
          // Language is ignored.
          break;
        case 2:
          temp_value = t.token();
          break;
        default:
          return false;
      }
    }
  }
  if (numDelimsSeen != 2)
    return false;
  if (temp_charset.empty() || temp_value.empty())
    return false;
  charset->swap(temp_charset);
  value_chars->swap(temp_value);
  return true;
}

// http://tools.ietf.org/html/rfc5987#section-3.2
//
//  ext-value     = charset  "'" [ language ] "'" value-chars
//
//  charset       = "UTF-8" / "ISO-8859-1" / mime-charset
//
//  mime-charset  = 1*mime-charsetc
//  mime-charsetc = ALPHA / DIGIT
//                 / "!" / "#" / "$" / "%" / "&"
//                 / "+" / "-" / "^" / "_" / "`"
//                 / "{" / "}" / "~"
//
//  language      = <Language-Tag, defined in [RFC5646], Section 2.1>
//
//  value-chars   = *( pct-encoded / attr-char )
//
//  pct-encoded   = "%" HEXDIG HEXDIG
//
//  attr-char     = ALPHA / DIGIT
//                 / "!" / "#" / "$" / "&" / "+" / "-" / "."
//                 / "^" / "_" / "`" / "|" / "~"
bool DecodeExtValue(const std::string& param_value, std::string* decoded) {
  if (param_value.find('"') != std::string::npos)
    return false;

  std::string charset;
  std::string value;
  if (!ParseExtValueComponents(param_value, &charset, &value))
    return false;

  // RFC 5987 value should be ASCII-only.
  if (!IsStringASCII(value)) {
    decoded->clear();
    return true;
  }

  std::string unescaped = net::UnescapeURLComponent(
      value, net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);

  return base::ConvertToUtf8AndNormalize(unescaped, charset, decoded);
}

} // namespace

namespace net {

HttpContentDisposition::HttpContentDisposition(
    const std::string& header, const std::string& referrer_charset)
  : type_(INLINE),
    parse_result_flags_(INVALID) {
  Parse(header, referrer_charset);
}

HttpContentDisposition::~HttpContentDisposition() {
}

std::string::const_iterator HttpContentDisposition::ConsumeDispositionType(
    std::string::const_iterator begin, std::string::const_iterator end) {
  DCHECK(type_ == INLINE);
  std::string::const_iterator delimiter = std::find(begin, end, ';');

  std::string::const_iterator type_begin = begin;
  std::string::const_iterator type_end = delimiter;
  HttpUtil::TrimLWS(&type_begin, &type_end);

  // If the disposition-type isn't a valid token the then the
  // Content-Disposition header is malformed, and we treat the first bytes as
  // a parameter rather than a disposition-type.
  if (!HttpUtil::IsToken(type_begin, type_end))
    return begin;

  parse_result_flags_ |= HAS_DISPOSITION_TYPE;

  DCHECK(std::find(type_begin, type_end, '=') == type_end);

  if (LowerCaseEqualsASCII(type_begin, type_end, "inline")) {
    type_ = INLINE;
  } else if (LowerCaseEqualsASCII(type_begin, type_end, "attachment")) {
    type_ = ATTACHMENT;
  } else {
    parse_result_flags_ |= HAS_UNKNOWN_DISPOSITION_TYPE;
    type_ = ATTACHMENT;
  }
  return delimiter;
}

// http://tools.ietf.org/html/rfc6266
//
//  content-disposition = "Content-Disposition" ":"
//                         disposition-type *( ";" disposition-parm )
//
//  disposition-type    = "inline" | "attachment" | disp-ext-type
//                      ; case-insensitive
//  disp-ext-type       = token
//
//  disposition-parm    = filename-parm | disp-ext-parm
//
//  filename-parm       = "filename" "=" value
//                      | "filename*" "=" ext-value
//
//  disp-ext-parm       = token "=" value
//                      | ext-token "=" ext-value
//  ext-token           = <the characters in token, followed by "*">
//
void HttpContentDisposition::Parse(const std::string& header,
                                   const std::string& referrer_charset) {
  DCHECK(type_ == INLINE);
  DCHECK(filename_.empty());

  std::string::const_iterator pos = header.begin();
  std::string::const_iterator end = header.end();
  pos = ConsumeDispositionType(pos, end);

  std::string name;
  std::string filename;
  std::string ext_filename;

  HttpUtil::NameValuePairsIterator iter(pos, end, ';');
  while (iter.GetNext()) {
    if (filename.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                 iter.name_end(),
                                                 "filename")) {
      DecodeFilenameValue(iter.value(), referrer_charset, &filename,
                          &parse_result_flags_);
      if (!filename.empty())
        parse_result_flags_ |= HAS_FILENAME;
    } else if (name.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                    iter.name_end(),
                                                    "name")) {
      DecodeFilenameValue(iter.value(), referrer_charset, &name, NULL);
      if (!name.empty())
        parse_result_flags_ |= HAS_NAME;
    } else if (ext_filename.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                            iter.name_end(),
                                                            "filename*")) {
      DecodeExtValue(iter.raw_value(), &ext_filename);
      if (!ext_filename.empty())
        parse_result_flags_ |= HAS_EXT_FILENAME;
    }
  }

  if (!ext_filename.empty())
    filename_ = ext_filename;
  else if (!filename.empty())
    filename_ = filename;
  else
    filename_ = name;
}

}  // namespace net
