//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2017, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------

#include "tsTextFormatter.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Constructors and destructor.
//----------------------------------------------------------------------------

ts::TextFormatter::TextFormatter(Report& report) :
    std::basic_ostream<char>(this),
    std::basic_streambuf<char>(),
    _report(report),
    _outFile(),
    _outString(),
    _out(&_outFile), // _out is never null, points by default to a closed file (discard output)
    _margin(0),
    _indent(2),
    _curMargin(_margin),
    _tabSize(8),
    _column(0),
    _afterSpace(false),
    _buffer(256, '\0')
{
    resetBuffer();
}

ts::TextFormatter::~TextFormatter()
{
    close();
}


//----------------------------------------------------------------------------
// Set output to an open text stream.
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::setStream(std::ostream& strm)
{
    close();
    _out = &strm;
    return *this;
}


//----------------------------------------------------------------------------
// Set output to a text file.
//----------------------------------------------------------------------------

bool ts::TextFormatter::setFile(const UString& fileName)
{
    close();
    _outFile.open(fileName.toUTF8().c_str(), std::ios::out);
    if (!_outFile) {
        _report.error(u"cannot create file %s", {fileName});
        return false;
    }
    else {
        _out = &_outFile;
        return true;
    }
}


//----------------------------------------------------------------------------
// Set output to an internal string buffer. 
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::setString()
{
    close();
    _out = &_outString;
    return *this;
}


//----------------------------------------------------------------------------
// Retrieve the current contentn of the internal string buffer. 
//----------------------------------------------------------------------------

bool ts::TextFormatter::getString(UString& str)
{
    if (_out != &_outString) {
        // Output is not set to string.
        str.clear();
        return false;
    }
    else {
        // Get internal buffer, do not reset it.
        flush();
        str.assignFromUTF8(_outString.str());
        // Cleanup end of lines.
        str.substitute(UString(1, CARRIAGE_RETURN), UString());
        return true;
    }
}

ts::UString ts::TextFormatter::toString()
{
    UString str;
    getString(str);
    return str;
}


//----------------------------------------------------------------------------
// Check if the Output is open to some output.
//----------------------------------------------------------------------------

bool ts::TextFormatter::isOpen() const
{
    return _out != &_outFile || _outFile.is_open();
}


//----------------------------------------------------------------------------
// Close the current output.
//----------------------------------------------------------------------------

void ts::TextFormatter::close()
{
    if (_out == &_outString) {
        // Output is set to string. Reset internal buffer.
        _outString.str(std::string());
    }
    if (_outFile.is_open()) {
        _outFile.close();
    }
    // Set output to a closed file. Thus, _out is never null, it is safe to
    // output to *_out, but output is discarded (closed file).
    _out = &_outFile;
    // Reset margin.
    _curMargin = _margin;
}


//----------------------------------------------------------------------------
// Set the margin size for outer-most elements.
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::setMarginSize(size_t margin)
{
    // Try to adjust current margin by the same amount.
    if (margin > _margin) {
        _curMargin += margin - _margin;
    }
    else if (margin < _margin) {
        _curMargin -= std::min(_curMargin, _margin - margin);
    }

    // Set the new margin.
    _margin = margin;
    return *this;
}


//----------------------------------------------------------------------------
// This is called when buffer becomes full.
//----------------------------------------------------------------------------

int ts::TextFormatter::overflow(int c)
{
    // Flush content of the buffer.
    bool ok = flushBuffer();

    // Flush the character that didn't fit in buffer.
    if (ok && c != traits_type::eof()) {
        char ch = char(c);
        ok = flushData(&ch, &ch + 1);
    }

    // Nothing to flush anymore.
    resetBuffer();
    return ok ? c : traits_type::eof();
}


//----------------------------------------------------------------------------
// This function is called when the stream is flushed.
//----------------------------------------------------------------------------

int ts::TextFormatter::sync()
{
    const bool ok = flushBuffer();
    resetBuffer();
    return ok ? 0 : -1;
}


//----------------------------------------------------------------------------
// Flush data to underlying output.
//----------------------------------------------------------------------------

bool ts::TextFormatter::flushData(const char* firstAddr, const char* lastAddr)
{
    for (const char* p = firstAddr; p < lastAddr; ++p) {
        if (*p == '\t') {
            // Tabulations are expanded as spaces.
            while (++_column % _tabSize != 0) {
                *_out << ' ';
            }
        }
        else if (*p == '\r' || *p == '\n') {
            // CR and LF indifferently move back to begining of current/next line.
            *_out << *p;
            _column = 0;
            _afterSpace = false;
        }
        else {
            *_out << *p;
            ++_column;
            _afterSpace = _afterSpace || *p != ' ';
        }
    }
    return !_out->fail();
}


//----------------------------------------------------------------------------
// Insert all necessary new-lines and spaces to move to the current margin.
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::margin()
{
    // Flush pending output.
    sync();

    // New line if we are farther than the margin.
    // Also new line when we are no longer in the margin ("after space")
    // even if we do not exceed the margin size.
    if (_column > _curMargin || _afterSpace) {
        *_out << std::endl;
        _column = 0;
    }

    *_out << std::string(_curMargin - _column, ' ');
    _column = _curMargin;
    _afterSpace = false;
    return *this;
}


//----------------------------------------------------------------------------
// Insert all necessary new-lines and spaces to move to a given column.
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::column(size_t col)
{
    // Flush pending output.
    sync();

    // New line if we are farther than the target col.
    if (_column > col) {
        *_out << std::endl;
        _column = 0;
        _afterSpace = false;
    }

    *_out << std::string(col - _column, ' ');
    _column = col;
    return *this;
}


//----------------------------------------------------------------------------
// Output spaces on the stream.
//----------------------------------------------------------------------------

ts::TextFormatter& ts::TextFormatter::spaces(size_t count)
{
    // Flush pending output.
    sync();

    // Space after the buffer content.
    *_out << std::string(count, ' ');
    _column += count;
    return *this;
}

//----------------------------------------------------------------------------
// I/O manipulators.
//----------------------------------------------------------------------------

ts::IOManipulatorProxy<ts::TextFormatter, size_t> ts::margin(size_t size)
{
    return IOManipulatorProxy<TextFormatter, size_t>(&TextFormatter::setMarginSize, size);
}

ts::IOManipulatorProxy<ts::TextFormatter, size_t> ts::spaces(size_t count)
{
    return IOManipulatorProxy<TextFormatter, size_t>(&TextFormatter::spaces, count);
}

ts::IOManipulatorProxy<ts::TextFormatter, size_t> ts::column(size_t col)
{
    return IOManipulatorProxy<TextFormatter, size_t>(&TextFormatter::column, col);
}
