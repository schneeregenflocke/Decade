/*
Decade
Copyright (c) 2019-2020 Marco Peyer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>.
*/

#pragma once

#include "../calendar_config.h"

#ifdef WX_PRECOMP
#include <wx/wxprec.h>
#else 
#include <wx/wx.h>
#endif

#include <wx/sizer.h>
//#include <wx/stattext.h>
//#include <wx/spinctrl.h>
//#include <wx/checkbox.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/manager.h>

#include <sigslot/signal.hpp>

#include <pugixml.hpp>

#include <array>
#include <vector>
#include <memory>
#include <functional>
#include <iostream>


class PropertyGridPanel : public wxPropertyGrid
{
public:

	PropertyGridPanel(wxWindow* parent) :
		wxPropertyGrid(parent, -1, wxDefaultPosition, wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER),
		box_sizer(new wxBoxSizer(wxHORIZONTAL))	
	{
		auto sizer_flags = wxSizerFlags().Proportion(1).Expand();
		parent->GetSizer()->Add(box_sizer, sizer_flags);
		box_sizer->Add(this, sizer_flags);

		SetVerticalSpacing(2);

		Append(new wxPropertyCategory("Calendar Span", wxPG_LABEL));
		
		gui_auto_span = new wxBoolProperty("Auto", wxPG_LABEL, false);
		gui_lower_limit = new wxIntProperty("Lower Limit", wxPG_LABEL, 0);
		gui_upper_limit = new wxIntProperty("Upper Limit", wxPG_LABEL, 0);

		Append(gui_auto_span);
		Append(gui_lower_limit);
		Append(gui_upper_limit);
		DisableProperty(gui_lower_limit);
		DisableProperty(gui_upper_limit);


		Append(new wxPropertyCategory("Row Spacing Proportions", wxPG_LABEL));

		gui_number_spacings = new wxIntProperty("Number Spacings", wxPG_LABEL, 0);
		Append(gui_number_spacings);
		DisableProperty(gui_number_spacings);
		
		RefreshPropertyGrid();
	}

	void RefreshPropertyGrid()
	{
		bool auto_span = GetPropertyValue(gui_auto_span).GetBool();

		if (auto_span == true)
		{
			DisableProperty(gui_lower_limit);
			DisableProperty(gui_upper_limit);
		}
		if (auto_span == false)
		{
			EnableProperty(gui_lower_limit);
			EnableProperty(gui_upper_limit);
		}

		long number_spacings = GetPropertyValue(gui_number_spacings).GetInteger();

		if (number_spacings > gui_spacings_array.size())
		{
			for (size_t index = gui_spacings_array.size(); index < number_spacings; ++index)
			{
				int index_parity = index % 2;
				std::string label_number_postfix = std::to_string((index - index_parity) / 2 + 1);
				std::string label;

				if (index_parity == 0)
				{
					label = std::string("Gap ") + label_number_postfix;
				}
				if (index_parity == 1)
				{
					label = std::string("Subrow ") + label_number_postfix;
				}

				gui_spacings_array.push_back(new wxFloatProperty(label, wxPG_LABEL, 10.0));
				Append(gui_spacings_array[index]);
			}
		}

		if (number_spacings < gui_spacings_array.size())
		{
			for (size_t index = number_spacings; index < gui_spacings_array.size();)
			{
				DeleteProperty(gui_spacings_array[index]);
				gui_spacings_array.pop_back();
			}
		}
	}

	wxBoxSizer* box_sizer;
	
	wxBoolProperty* gui_auto_span;
	wxIntProperty* gui_lower_limit;
	wxIntProperty* gui_upper_limit;
	wxIntProperty* gui_number_spacings;
	std::vector<wxFloatProperty*> gui_spacings_array;
};


class CalendarSetupPanel : public wxPanel
{
public:

	CalendarSetupPanel(wxWindow* parent) :
		wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxPanelNameStr),
		property_grid(nullptr)
	{
		wxBoxSizer* vertical_sizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(vertical_sizer);

		property_grid = new PropertyGridPanel(this);

		Bind(wxEVT_PG_CHANGED, &CalendarSetupPanel::OnPropertyGridChanging, this);

		ActualizePropertyGridValues();
	}

	void OnPropertyGridChanging(wxPropertyGridEvent& event)
	{
		property_grid->RefreshPropertyGrid();

		//auto current_property = event.GetProperty();

		long number_subrows = property_grid->GetPropertyValue(property_grid->gui_number_spacings).GetInteger();
		calendar_config.subrow_proportions.resize(number_subrows);

		for (size_t index = 0; index < number_subrows; ++index)
		{
			calendar_config.subrow_proportions[index] = static_cast<float>(property_grid->GetPropertyValue(property_grid->gui_spacings_array[index]).GetDouble());
		}

		calendar_config.auto_calendar_range = property_grid->GetPropertyValue(property_grid->gui_auto_span).GetBool();

		long lower_limit = property_grid->GetPropertyValue(property_grid->gui_lower_limit).GetInteger();
		long upper_limit = property_grid->GetPropertyValue(property_grid->gui_upper_limit).GetInteger();
		
		calendar_config.SetCalendarRange(lower_limit, upper_limit);

		signal_calendar_config(calendar_config);
	}

	void ActualizePropertyGridValues()
	{
		property_grid->SetPropertyValue(property_grid->gui_number_spacings, static_cast<int>(calendar_config.subrow_proportions.size()));

		property_grid->RefreshPropertyGrid();

		for (size_t index = 0; index < calendar_config.subrow_proportions.size(); ++index)
		{
			property_grid->SetPropertyValue(property_grid->gui_spacings_array[index], calendar_config.subrow_proportions[index]);
		}
		
		property_grid->SetPropertyValue(property_grid->gui_auto_span, calendar_config.auto_calendar_range);
		property_grid->SetPropertyValue(property_grid->gui_lower_limit, calendar_config.GetCalendarRange().first);
		property_grid->SetPropertyValue(property_grid->gui_upper_limit, calendar_config.GetCalendarRange().second);
	}
	
	void SendCalendarConfig()
	{
		signal_calendar_config(calendar_config);
	}

	void LoadXML(const pugi::xml_node& node);
	void SaveXML(pugi::xml_node* node);
	
	sigslot::signal<const CalendarConfig&> signal_calendar_config;

private:

	PropertyGridPanel* property_grid;
	CalendarConfig calendar_config;
};
