/*

Copyright (c) 2015, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#ifndef WOBBLYEXCEPTION_H
#define WOBBLYEXCEPTION_H

#include <QString>
#include <stdexcept>
#include <string>


class WobblyException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    WobblyException(const QString &text) : std::runtime_error(text.toStdString()) { }
};

#endif // WOBBLYEXCEPTION_H

