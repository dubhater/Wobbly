/*

Copyright (c) 2018, John Smith

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


#ifndef RANDOMSTUFF_H
#define RANDOMSTUFF_H

#include <string>
#include <vector>


static std::string handleSingleQuotes(const std::string &path) {
    // Turn <afkjhg'sgsh'fhdfh> into <afkjhg r"'" r'sgsh' r"'" r'fhdfh>
    // Replace every ' in the string with ' r"'" r' so that the file name can be passed to Python.
    // Opening and closing single quotes are provided by the caller.

    char needle = '\'';
    std::string replacement("' r\"'\" r'");

    std::vector<size_t> positions;

    size_t position = 0;
    while (true) {
        position = path.find(needle, position);

        if (position == std::string::npos)
            break;
        else {
            positions.push_back(position);
            position++;
        }
    }

    std::string fixed_path = path;

    for (int i = positions.size() - 1; i >= 0; i--)
        fixed_path.replace(positions[i], 1, replacement);

    return fixed_path;
}

#endif // RANDOMSTUFF_H
