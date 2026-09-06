/**
 * @file SetupResources.h
 * @author Christian Schenk
 * @brief Setup resources
 *
 * @copyright Copyright © 2023 Christian Schenk
 *
 * This file is part of the MiKTeX Setup Library.
 *
 * The MiKTeX Setup Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#pragma once

#include "config.h"

#include <miktex/Resources/ResourceRepository>

BEGIN_INTERNAL_NAMESPACE;

// #include "C:/ddksample/miktex/output/Libraries/MiKTeX/Setup/locale/de/miktex-setup.mo.h"
// #include "C:/ddksample/miktex/output/Libraries/MiKTeX/Setup/locale/zh_CN/miktex-setup.mo.h"
#include "locale/de/miktex-setup.mo.h"
#include "locale/zh_CN/miktex-setup.mo.h"


class SetupResources
    : public MiKTeX::Resources::ResourceRepository
{

protected:

    void Init() override
    {
        R_(":/de/LC_MESSAGES/miktex-setup.mo", messages_de);
R_(":/zh_CN/LC_MESSAGES/miktex-setup.mo", messages_zh_CN);

    }

private:

    template<std::size_t N> void R_(const char* resourceId, unsigned char const (&byteArray)[N])
    {
        AddResource(resourceId, { byteArray, sizeof(byteArray) });
    }
};

END_INTERNAL_NAMESPACE;
