#ifndef WOBBLYEXCEPTION_H
#define WOBBLYEXCEPTION_H

#include <QString>
#include <stdexcept>
#include <string>


class WobblyException : public std::runtime_error {
public:
    WobblyException(const char *text) : std::runtime_error(text) { }
    WobblyException(const std::string &text) : std::runtime_error(text) { }
    WobblyException(const QString &text) : std::runtime_error(text.toStdString()) { }
};

#endif // WOBBLYEXCEPTION_H

