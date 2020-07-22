#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include "bitmap_font.h"


class AbstractExporter
{

public:
    explicit AbstractExporter() = default;

    const QString& getErrorString() const { return "m_error_string";}

    virtual str apply(bitmap_font & data) = 0;
    virtual str tag() = 0;
};

