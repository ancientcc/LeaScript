/**
 * @file AppResources.h
 * @author Christian Schenk
 * @brief Application resources
 *
 * @copyright Copyright © 2005-2022 Christian Schenk
 *
 * This file is part of the MiKTeX Application Framework.
 *
 * The MiKTeX Application Framework is licensed under GNU General Public License
 * version 2 or any later version.
 */

#pragma once

#include "miktex_App_config.h"

#include <miktex/Resources/ResourceRepository>

BEGIN_INTERNAL_NAMESPACE;

// #include "C:/ddksample/miktex/output/Libraries/MiKTeX/App/locale/de/miktex-app.mo.h"
// #include "C:/ddksample/miktex/output/Libraries/MiKTeX/App/locale/zh_CN/miktex-app.mo.h"
#include "locale/de/miktex-app.mo.h"
#include "locale/zh_CN/miktex-app.mo.h"


class AppResources
    : public MiKTeX::Resources::ResourceRepository
{

protected:

    void Init() override
    {
        R_(":/de/LC_MESSAGES/miktex-app.mo", messages_de);
R_(":/zh_CN/LC_MESSAGES/miktex-app.mo", messages_zh_CN);

    }

private:

    template<std::size_t N> void R_(const char* resourceId, unsigned char const (&byteArray)[N])
    {
        AddResource(resourceId, { byteArray, sizeof(byteArray) });
    }
};

END_INTERNAL_NAMESPACE;
