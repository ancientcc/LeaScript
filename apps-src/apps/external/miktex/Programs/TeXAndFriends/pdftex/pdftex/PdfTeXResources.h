/**
 * @file PdfTeXResources.h
 * @author Christian Schenk
 * @brief MiKTeX-pdfTeX resources
 *
 * @copyright Copyright © 2021-2022 Christian Schenk
 *
 * This file is free software; the copyright holder gives unlimited permission
 * to copy and/or distribute it, with or without modifications, as long as this
 * notice is preserved.
 */

#pragma once

#include <miktex/Resources/ResourceRepository>

// #include "C:/ddksample/miktex/output/Programs/TeXAndFriends/pdftex/pdftex/locale/de/miktex-pdftex.mo.h"
// #include "C:/ddksample/miktex/output/Programs/TeXAndFriends/pdftex/pdftex/locale/zh_CN/miktex-pdftex.mo.h"
#include "locale/de/miktex-pdftex.mo.h"
#include "locale/zh_CN/miktex-pdftex.mo.h"


class PdfTeXResources
  : public MiKTeX::Resources::ResourceRepository
{
protected:
  void Init() override
  {
    R_(":/de/LC_MESSAGES/miktex-pdftex.mo", messages_de);
R_(":/zh_CN/LC_MESSAGES/miktex-pdftex.mo", messages_zh_CN);

  }

private:
  template<std::size_t N> void R_(const char* resourceId, unsigned char const (&byteArray)[N])
  {
    AddResource(resourceId, { byteArray, sizeof(byteArray) });
  }
};
