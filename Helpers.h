#ifndef HELPERS_H
#define HELPERS_H

#include "Cryption.h"
#include "Vector.h"
#include "UniquePointer.h"

#include <cctype>
#include <iostream>
#include <limits>

#include <stdio.h>

namespace Helpers
{

	void parseUserInput(const char* userInput, Vault& vault)
	{
		//get length of user input for init dynamic c-string
		std::size_t len = strlen(userInput);

		//init buffer, wrap in UniquePtr so dont have to call delete[] later
		UniquePtr<char[]> buffer = makeUnique<char[]>(len + 1);
		//copy data
		strcpy_s(buffer.get(), len + 1, userInput);
		//buffer, userInput)

		//pointing to null terminated buffer
		char* p = buffer.get();

		//skip leading whitespace to get to [command]
		while (*p && std::isspace(static_cast<unsigned char>(*p)))
			++p;

		//help prevent unable to parse message from deleteEntryAndSave
		if (*p == '\0')
			return;

		//start of [command]
		char* cmdStart = p;

		//there is a dereferencable char and it is NOT a space and NOT '(', advance. isolates [command]
		while (*p && !std::isspace(static_cast<unsigned char>(*p)) && *p != '(')
			++p;

		//copy [command] into temp buffer
		std::size_t cmdLength = p - cmdStart;

		if (cmdLength >= 16)
			throw std::runtime_error("Command length too long.");

		//store [command] retrieved from cmdStart
		char cmd[16]{}; 
		std::memcpy(cmd, cmdStart, cmdLength);
		//append null terminator
		cmd[cmdLength] = '\0';

		//cast to lowercase, ptr to first byte in cmd, can dereference c, advance c
		for (char* c = cmd; *c; ++c)
			//dereference c, cast it to unsigned char for to lower, then back to a normal char for =operator
			*c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));

		//helper for lambda err on unexpected null terminator
		auto unxptdTerm = [&](char* ptr) -> void
			{
				if(*ptr == '\0')
					throw std::runtime_error("Unexpected null terminator while reading param1");
			};

		//returns dynamically allocated memory, MUST free
		auto truncateStart = [&]()
			{
				//remove whitespace after [command]
				while (*p && std::isspace(static_cast<unsigned char>(*p)))
					++p;

				//must have '(' as next char
				if (*p != '(')
					throw std::runtime_error("Incorrect format, must be add(username, website, password) with or without whitespace.");

				//consume '('
				++p;

				//consume leading whitespace before [param1]
				while (*p && std::isspace(static_cast<unsigned char>(*p)))
					++p;

				//start of parameter 1
				char* param1Start = p;

				//read until we reach next param symbol: ',' or an end symbol ')'
				while (*p && *p != ',' && *p != ')')
					++p;

				//check for unexpected null terminator
				unxptdTerm(p);

				//points to ',' or ')'. One char beyond end of actual param1
				char* param1End = p;

				//param1End[-1] is the same as * (param1End - 1). Avoids dereferencing
				while (param1End > param1Start && std::isspace(static_cast<unsigned char>(param1End[-1])))
					--param1End;

				std::size_t param1Length = static_cast<std::size_t>(param1End - param1Start);
				if (param1Length == 0)
					throw std::runtime_error("Parameter 1 is empty");

				//wrap in uniqueptr so dont have to worry about mem
				//char* param1 = new char[param1Length + 1];
				UniquePtr<char[]> param1 = makeUnique<char[]>(param1Length + 1);

				std::memcpy(param1.get(), param1Start, param1Length);
				param1[param1Length] = '\0';

				return param1;
			};

		auto truncateX= [&]()
			{
				//consume ','
				++p;

				//trim leading whitespace before param in case of (param1, param2...
				while (*p && std::isspace(static_cast<unsigned char>(*p)))
					++p;

				//start of param2
				char* param2Start = p;

				//read until we reach next param symbol: ',' or ')' marking end
				while (*p && *p != ',' && *p != ')')
					++p;

				unxptdTerm(p);

				//end of param2 
				char* param2End = p;

				//remove trailing whitespace
				while (param2End > param2Start && std::isspace(static_cast<unsigned char>(param2End[-1])))
					--param2End;

				std::size_t param2Length = static_cast<std::size_t>(param2End - param2Start);
				if (param2Length == 0)
					throw std::runtime_error("Empty param2");

				//char* param2 = new char[param2Length + 1];
				UniquePtr<char[]> param2 = makeUnique<char[]>(param2Length + 1);

				std::memcpy(param2.get(), param2Start, param2Length);
				param2[param2Length] = '\0';

				return param2;
			};

		auto truncateStartNum = [&]()
			{
				//remove whitespace after [command]
				while (*p && std::isspace(static_cast<unsigned char>(*p)))
					++p;

				//must have '(' as next char
				if (*p != '(')
					throw std::runtime_error("Incorrect format, must be add(username, website, password) with or without whitespace.");
			};

		auto truncateNum = [&]()
			{
				//consume '(' or ','
				++p;

				//consume leading whitespace before [param1]
				while (*p && std::isspace(static_cast<unsigned char>(*p)))
					++p;

				//start of parameter 1
				char* param1Start = p;

				//read until we reach next param symbol: ',' or an end symbol ')'
				while (*p && *p != ',' && *p != ')')
					++p;

				//points to ',' or ')'. One char beyond end of actual param1
				char* param1End = p;

				//param1End[-1] is the same as * (param1End - 1). Avoids dereferencing
				while (param1End > param1Start && std::isspace(static_cast<unsigned char>(param1End[-1])))
					--param1End;

				char* ptr = param1Start;

				if (param1Start == param1End)
					throw std::runtime_error("Empty index");

				std::size_t value = 0;

				for (char* ptr{ param1Start }; ptr < param1End; ++ptr)
				{
					//numeric index check
					if (!std::isdigit(static_cast<unsigned char>(*ptr)))
						throw std::runtime_error("Index must be numeric");

					//convert char to int
					int digit = *ptr - '0';

					//shift the number left by one decimal place and add the new digit. IE 123 -> 0*10+1 -> 1 -> 1*10+2 -> 12 -> 12*10+3 -> 123
					value = value * 10 + digit;
				}
				return value;
			};

		if (strcmp(cmd, "display") == 0)
			vault.listAllEntries();

		else if (strcmp(cmd, "add") == 0)
		{
			//extract param1
			UniquePtr<char[]> param1 = truncateStart();
			//extract param2
			UniquePtr<char[]> param2 = truncateX();
			//extract param3
			UniquePtr<char[]> param3 = truncateX();

			//delegate to addEntryAndSave with extracted params
			vault.addEntryAndSave(param1.get(), param2.get(), param3.get());
		}
		else if (strcmp(cmd, "edit") == 0)
		{
			//p -> edit(HERE.....) after parseStart
			truncateStartNum();

			//returns user-passed index as std::size_t
			std::size_t index = truncateNum();

			//retrieve Entry at index of vault
			Entry& et = vault.get(index);
			char temp[64]; //stack allocate

			//if the user entered nothing, dont adjust it. If they did, adjust field to new user string
			auto updateField = [&](char*& field)
				{
					if (temp[0] != '\0')
					{
						delete[] field;

						std::size_t dstSize = strlen(temp) + 1;
						field = new char[dstSize];
						strcpy_s(field, dstSize, temp); //destination, destinationSize, source
					}
				};

			//adj website
			std::cout << "Website [" << et.website_ << "]: ";
			std::cin.getline(temp, sizeof(temp));
			updateField(et.website_);

			//adj ussername
			std::cout << "Username [" << et.username_ << "]: ";
			std::cin.getline(temp, sizeof(temp));
			updateField(et.username_);

			//adj password
			std::cout << "Password [" << et.password_ << "]: ";
			std::cin.getline(temp, sizeof(temp));
			updateField(et.password_);

			//overloaded for taking an entry
			vault.editAndSave(index, et);
		}
		else if (strcmp(cmd, "delete") == 0)
		{
			//init storage
			Vector<std::size_t> storage;

			truncateStartNum();

			//emplace first param back
			storage.emplace_back(truncateNum());

			//if there is another param, extract, emplace, until no more params
			while(*p == ',')
				//extract and store the param
				storage.emplace_back(truncateNum());

			vault.deleteEntryAndSave(storage);
		}
		else if (strcmp(cmd, "cmds") == 0 || strcmp(cmd, "cmd") == 0)
		{
			vault.displayCmds();
		}
		else
		{
			std::cout << "Unable to parse user input\n";
		}
	}
}




#endif