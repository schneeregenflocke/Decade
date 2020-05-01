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


#include "date_group_store.h"

// call after connecting
void DateGroupStore::InitDefaults()
{
	date_groups.push_back(DateGroup(0, L"Default"));
	date_groups.push_back(DateGroup(0, L"Single"));

	UpdateNumbers();

	signal_date_groups(date_groups);
}

void DateGroupStore::SetDateGroups(const std::vector<DateGroup>& date_groups)
{
	this->date_groups = date_groups;

	UpdateNumbers();

	signal_date_groups(this->date_groups);
}

void DateGroupStore::UpdateNumbers()
{
	int number = 1;
	for (auto& date_group : date_groups)
	{
		date_group.number = number;
		++number;
	}
}


