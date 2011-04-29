//  tinygettext - A gettext replacement that works directly on .po files
//  Copyright (C) 2006 Ingo Ruhnke <grumbel@gmx.de>
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include <assert.h>
#include "dictionary.hpp"

namespace tinygettext {

Dictionary::Dictionary(const std::string& charset_) :
          entries(),
          ctxt_entries(),
          charset(charset_),
          plural_forms()
{
}

Dictionary::~Dictionary()
{
}

std::string Dictionary::get_charset() const
{
    return charset;
}

void Dictionary::set_plural_forms(const PluralForms& plural_forms_)
{
    plural_forms = plural_forms_;
}

PluralForms Dictionary::get_plural_forms() const
{
    return plural_forms;
}

std::string Dictionary::translate_plural(const std::string& msgid, const std::string& msgid_plural, int num)
{
    return translate_plural(entries, msgid, msgid_plural, num);
}

std::string Dictionary::translate_plural(const Entries& dict, const std::string& msgid, const std::string& msgid_plural, int count)
{
    Entries::const_iterator i = dict.find(msgid);
    const std::vector<std::string>& msgstrs = i->second;

    if (i != dict.end())
    {
        unsigned int n = plural_forms.get_plural(count);
        assert(n < msgstrs.size());

        return (!msgstrs[n].empty())
                ? msgstrs[n]
                : (count == 1)
                          ? msgid
                          : msgid_plural;
    }
    else
    {
        return (count == 1)
                ? msgid
                : msgid_plural;
    }
}

std::string
Dictionary::translate(const std::string& msgid)
{
    return translate(entries, msgid);
}

std::string Dictionary::translate(const Entries& dict, const std::string& msgid)
{
    Entries::const_iterator i = dict.find(msgid);
    return (i != dict.end() && !i->second.empty())
            ? i->second[0]
            : msgid;
}

std::string Dictionary::translate_ctxt(const std::string& msgctxt, const std::string& msgid)
{
    CtxtEntries::iterator i = ctxt_entries.find(msgctxt);
    return (i != ctxt_entries.end())
            ? translate(i->second, msgid)
            : msgid;
}

std::string Dictionary::translate_ctxt_plural(const std::string& msgctxt,
        const std::string& msgid, const std::string& msgidplural, int num)
{
    CtxtEntries::iterator i = ctxt_entries.find(msgctxt);
    return (i != ctxt_entries.end())
            ? translate_plural(i->second, msgid, msgidplural, num)
            : (num != 1)
                    ? msgidplural
                    : msgid;
}

void Dictionary::add_translation(const std::string& msgid, const std::string& ,
        const std::vector<std::string>& msgstrs)
{
    entries[msgid] = msgstrs;
}

void Dictionary::add_translation(const std::string& msgid, const std::string& msgstr)
{
    std::vector<std::string>& vec = entries[msgid];
    if (vec.empty())
    {
        vec.push_back(msgstr);
    }
    else
    {
        vec[0] = msgstr;
    }
}

void Dictionary::add_translation(const std::string& msgctxt,
        const std::string& msgid, const std::string& msgid_plural,
        const std::vector<std::string>& msgstrs)
{
    std::vector<std::string>& vec = ctxt_entries[msgctxt][msgid];
    vec = msgstrs;
}

void Dictionary::add_translation(const std::string& msgctxt,
        const std::string& msgid, const std::string& msgstr)
{
    std::vector<std::string>& vec = ctxt_entries[msgctxt][msgid];
    if (vec.empty())
    {
        vec.push_back(msgstr);
    }
    else
    {
        vec[0] = msgstr;
    }
}

} // namespace tinygettext

/* EOF */
