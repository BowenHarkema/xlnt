// Copyright (c) 2014-2016 Thomas Fussell
// Copyright (c) 2010-2015 openpyxl
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, WRISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE
//
// @license: http://www.opensource.org/licenses/mit-license.php
// @author: see AUTHORS file
#pragma once

#include <list>
#include <string>
#include <vector>

#include <detail/format_impl.hpp>
#include <detail/style_impl.hpp>
#include <xlnt/cell/cell.hpp>
#include <xlnt/styles/format.hpp>
#include <xlnt/styles/style.hpp>
#include <xlnt/utils/exceptions.hpp>
#include <xlnt/worksheet/worksheet.hpp>
#include <xlnt/workbook/workbook.hpp>
#include <xlnt/workbook/worksheet_iterator.hpp>

namespace xlnt {
namespace detail {

struct stylesheet
{
    format create_format()
    {
		format_impls.push_back(format_impl());
		auto &impl = format_impls.back();

		impl.parent = this;
		impl.id = format_impls.size() - 1;
        
        impl.border_id = 0;
        impl.fill_id = 0;
        impl.font_id = 0;
        impl.number_format_id = 0;
        
        return format(&impl);
    }

	format get_format(std::size_t index)
	{
        auto iter = format_impls.begin();
        std::advance(iter, index);
		return format(&*iter);
	}
    
    class style create_style(const std::string &name)
    {
        auto &impl = style_impls.emplace(name, style_impl()).first->second;

		impl.parent = this;
        impl.name = name;

        impl.border_id = 0;
        impl.fill_id = 0;
        impl.font_id = 0;
        impl.number_format_id = 0;

        style_names.push_back(name);
        return xlnt::style(&impl);
    }

    class style style(const std::string &name)
	{
        if (!has_style(name)) throw key_not_found();
        return xlnt::style(&style_impls[name]);
	}

	bool has_style(const std::string &name)
	{
		return style_impls.count(name) > 0;
	}

	std::size_t next_custom_number_format_id() const
	{
		std::size_t id = 164;

		for (const auto &nf : number_formats)
		{
			if (nf.get_id() >= id)
			{
				id = nf.get_id() + 1;
			}
		}

		return id;
	}
    
    template<typename T, typename C>
    std::size_t find_or_add(C &container, const T &item)
    {
        std::size_t i = 0;
        
        for (auto iter = container.begin(); iter != container.end(); ++iter)
        {
            if (*iter == item)
            {
                return i;
            }

            ++i;
        }
        
        container.emplace(container.end(), item);

        return container.size() - 1;
    }
    
    template<typename T>
    std::unordered_map<std::size_t, std::size_t> garbage_collect(
        const std::unordered_map<std::size_t, std::size_t> &reference_counts,
        std::vector<T> &container)
    {
        std::unordered_map<std::size_t, std::size_t> id_map;
        std::size_t unreferenced = 0;
        const auto original_size = container.size();

        for (std::size_t i = 0; i < original_size; ++i)
        {
            id_map[i] = i - unreferenced;

            if (reference_counts.count(i) == 0 || reference_counts.at(i) == 0)
            {
                container.erase(container.begin() + i - unreferenced);
                unreferenced++;
            }
        }

        return id_map;
    }
    
    void garbage_collect(std::size_t new_format, std::size_t old_format)
    {
        if (!garbage_collection_enabled) return;

        std::unordered_map<std::size_t, std::size_t> format_reference_counts;
        
        for (const auto &impl : format_impls)
        {
            format_reference_counts[impl.id] = 0;
        }
        
        format_reference_counts[new_format]++;
        
        parent->apply_to_cells([&format_reference_counts](cell c)
        {
            if (c.has_format())
            {
                format_reference_counts[c.format().id()]++;
            }
        });
        
        if (format_reference_counts[old_format] > 0)
        {
            format_reference_counts[old_format]--;
        }
        
        for (const auto &id_count_pair : format_reference_counts)
        {
            if (id_count_pair.second != 0) continue;

            auto target_id = id_count_pair.first;
            auto target = std::find_if(format_impls.begin(), format_impls.end(),
                [=](const format_impl &impl) { return impl.id == target_id; });
            format_impls.erase(target);
        }
        
        std::size_t new_id = 0;

        std::unordered_map<std::size_t, std::size_t> alignment_reference_counts;
        std::unordered_map<std::size_t, std::size_t> border_reference_counts;
        std::unordered_map<std::size_t, std::size_t> fill_reference_counts;
        std::unordered_map<std::size_t, std::size_t> font_reference_counts;
        std::unordered_map<std::size_t, std::size_t> number_format_reference_counts;
        std::unordered_map<std::size_t, std::size_t> protection_reference_counts;
        
        for (auto &impl : format_impls)
        {
            impl.id = new_id++;
            
            if (impl.alignment_id.is_set())
            {
                alignment_reference_counts[impl.alignment_id.get()]++;
            }
            
            if (impl.border_id.is_set())
            {
                border_reference_counts[impl.border_id.get()]++;
            }
            
            if (impl.fill_id.is_set())
            {
                fill_reference_counts[impl.fill_id.get()]++;
            }
            
            if (impl.font_id.is_set())
            {
                font_reference_counts[impl.font_id.get()]++;
            }
            
            if (impl.number_format_id.is_set())
            {
                number_format_reference_counts[impl.number_format_id.get()]++;
            }
            
            if (impl.protection_id.is_set())
            {
                protection_reference_counts[impl.protection_id.get()]++;
            }
        }
        
        for (auto &name_impl_pair : style_impls)
        {
            auto &impl = name_impl_pair.second;
            
            if (impl.alignment_id.is_set())
            {
                alignment_reference_counts[impl.alignment_id.get()]++;
            }
            
            if (impl.border_id.is_set())
            {
                border_reference_counts[impl.border_id.get()]++;
            }
            
            if (impl.fill_id.is_set())
            {
                fill_reference_counts[impl.fill_id.get()]++;
            }
            
            if (impl.font_id.is_set())
            {
                font_reference_counts[impl.font_id.get()]++;
            }
            
            if (impl.number_format_id.is_set())
            {
                number_format_reference_counts[impl.number_format_id.get()]++;
            }
            
            if (impl.protection_id.is_set())
            {
                protection_reference_counts[impl.protection_id.get()]++;
            }
        }
        
        auto alignment_id_map = garbage_collect(alignment_reference_counts, alignments);
        auto border_id_map = garbage_collect(border_reference_counts, borders);
        auto fill_id_map = garbage_collect(fill_reference_counts, fills);
        auto font_id_map = garbage_collect(font_reference_counts, fonts);
        auto number_format_id_map = garbage_collect(number_format_reference_counts, number_formats);
        auto protection_id_map = garbage_collect(protection_reference_counts, protections);

        for (auto &impl : format_impls)
        {
            if (impl.alignment_id.is_set())
            {
                impl.alignment_id = alignment_id_map[impl.alignment_id.get()];
            }
            
            if (impl.border_id.is_set())
            {
                impl.border_id = border_id_map[impl.border_id.get()];
            }
            
            if (impl.fill_id.is_set())
            {
                impl.fill_id = fill_id_map[impl.fill_id.get()];
            }
            
            if (impl.font_id.is_set())
            {
                impl.font_id = font_id_map[impl.font_id.get()];
            }
            
            if (impl.number_format_id.is_set())
            {
                impl.number_format_id = number_format_id_map[impl.number_format_id.get()];
            }
            
            if (impl.protection_id.is_set())
            {
                impl.protection_id = protection_id_map[impl.protection_id.get()];
            }
        }
    }

    format_impl *find_or_create(const format_impl &pattern)
    {
        auto iter = format_impls.begin();
        auto id = find_or_add(format_impls, pattern);
        std::advance(iter, id);
        
        auto &result = *iter;

        result.parent = this;
        result.id = id;
        
        if (id != pattern.id)
        {
            garbage_collect(id, pattern.id);
        }

        return &result;
    }

    format_impl *find_or_create_with(format_impl *pattern, const alignment &new_alignment, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.alignment_id = find_or_add(alignments, new_alignment);
        new_format.alignment_applied = applied;
        
        return find_or_create(new_format);
    }

    format_impl *find_or_create_with(format_impl *pattern, const border &new_border, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.border_id = find_or_add(borders, new_border);
        new_format.border_applied = applied;
        
        return find_or_create(new_format);
    }
    
    format_impl *find_or_create_with(format_impl *pattern, const fill &new_fill, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.fill_id = find_or_add(fills, new_fill);
        new_format.fill_applied = applied;
        
        return find_or_create(new_format);
    }
    
    format_impl *find_or_create_with(format_impl *pattern, const font &new_font, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.font_id = find_or_add(fonts, new_font);
        new_format.font_applied = applied;
        
        return find_or_create(new_format);
    }
    
    format_impl *find_or_create_with(format_impl *pattern, const number_format &new_number_format, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.number_format_id = find_or_add(number_formats, new_number_format);
        new_format.number_format_applied = applied;
        
        return find_or_create(new_format);
    }
    
    format_impl *find_or_create_with(format_impl *pattern, const protection &new_protection, bool applied)
    {
        format_impl new_format = *pattern;
        new_format.protection_id = find_or_add(protections, new_protection);
        new_format.protection_applied = applied;
        
        return find_or_create(new_format);
    }

    std::size_t style_index(const std::string &name) const
    {
        return std::distance(style_names.begin(),
            std::find(style_names.begin(), style_names.end(), name));
    }
    
    void clear()
    {
        format_impls.clear();
        
        style_impls.clear();
        style_names.clear();
        
        alignments.clear();
        borders.clear();
        fills.clear();
        fonts.clear();
        number_formats.clear();
        protections.clear();
        
        colors.clear();
    }
    
    void enable_garbage_collection()
    {
        garbage_collection_enabled = true;
    }

    void disable_garbage_collection()
    {
        garbage_collection_enabled = false;
    }

    workbook *parent;
    
    bool garbage_collection_enabled = true;

    std::list<format_impl> format_impls;
    std::unordered_map<std::string, style_impl> style_impls;
    std::vector<std::string> style_names;

	std::vector<alignment> alignments;
    std::vector<border> borders;
    std::vector<fill> fills;
    std::vector<font> fonts;
    std::vector<number_format> number_formats;
	std::vector<protection> protections;
    
    std::vector<color> colors;
};

} // namespace detail
} // namespace xlnt
