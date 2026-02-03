#ifndef CRYPTION_H
#define CRYPTION_H

#include "Vector.h"
#include "UniquePointer.h"
#include "Sort.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdint>
#include <limits>
#include <iostream>
#include <type_traits>

#include <stdio.h>
#include <stdint.h>

#define NOMINMAX //otherwise limits ::max() gets polluted by windows max and min
#include <Windows.h>
#include <Wincrypt.h>
#include <dpapi.h>

namespace Crypto
{
	Vector<uint8_t> encryptData(const Vector<uint8_t>& plainBytesVector)
	{
		//max value a DWORD can safely store
		constexpr DWORD DWORD_MAX = (std::numeric_limits<DWORD>::max)();

		//size constraint, size cannot be more than the max value a DWORD can hold (0xFFFFFFFF)
		if (plainBytesVector.size() > DWORD_MAX)
			throw std::runtime_error("Out of bounds size");

		//init input and output blobs
		DATA_BLOB inBlob;
		inBlob.cbData = static_cast<DWORD>(plainBytesVector.size()); //number of bytes in Vector, cbData expects DWORD
		inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<uint8_t*>(plainBytesVector.data())); //ptr to first plaintext byte, pbData expects BYTE*

		DATA_BLOB outBlob;
		outBlob.cbData = 0; //number of bytes
		outBlob.pbData = nullptr; // pointer to first byte

		//convert from in to out, and check for success
		if (!CryptProtectData(&inBlob, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
			throw std::runtime_error("Encryption failed");

		//encrypted Vector to own data
		Vector<uint8_t> encrypted(outBlob.cbData);
		uint8_t* cursor = encrypted.data();

		//move the data from the outBlob to the Vector encrypted
		std::memcpy(cursor, outBlob.pbData, outBlob.cbData);

		//free pbData pointer
		LocalFree(outBlob.pbData);

		return encrypted;
	}

	Vector<uint8_t> decryptData(const Vector <uint8_t>& encrypted)
	{
		constexpr DWORD DWORD_MAX = (std::numeric_limits<DWORD>::max)();

		if (encrypted.size() > DWORD_MAX || encrypted.size() == 0)
			throw std::runtime_error("Out of bounds size");

		DATA_BLOB inBlob;
		inBlob.cbData = static_cast<DWORD>(encrypted.size()); //number of bytes in Vector, cbData expects DWORD
		inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<uint8_t*>(encrypted.data())); //ptr to first plaintext byte, pbData expects BYTE*

		DATA_BLOB outBlob;
		outBlob.cbData = 0;
		outBlob.pbData = nullptr;

		if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
			throw std::runtime_error("Unencryption phase failed");

		Vector<uint8_t> unencrypted(outBlob.cbData);
		uint8_t* cursor = unencrypted.data();

		std::memcpy(cursor, outBlob.pbData, outBlob.cbData);

		LocalFree(outBlob.pbData);

		return unencrypted;
	}
};

//owns memory for each entry, MUST deallocate mem
struct Entry
{
	char* website_ = nullptr;
	char* username_ = nullptr;
	char* password_ = nullptr;

	//disable copying
	Entry(const Entry&) = delete;
	Entry& operator= (const Entry&) = delete;

	Entry(const char* website, const char* username, const char* password)
	{
		const std::size_t websiteLength = strlen(website) + 1; //+1 for null terminator
		website_ = new char[websiteLength];
		std::memcpy(website_, website, websiteLength); //copy mem from website into website_, avoids dangling ptr from website_ = website;

		const std::size_t usernameLength = strlen(username) + 1;
		username_ = new char[usernameLength];
		std::memcpy(username_, username, usernameLength);

		const std::size_t passwordLength = strlen(password) + 1;
		password_ = new char[passwordLength];
		std::memcpy(password_, password, passwordLength);
	}

	//move constructor
	Entry(Entry&& other) noexcept
		: website_{ other.website_ }, username_{ other.username_ }, password_{ other.password_ }
	{
		other.website_ = nullptr;
		other.username_ = nullptr;
		other.password_ = nullptr;
	}

	//deep move assignment operator
	Entry& operator= (Entry&& other) noexcept
	{
		if (&other == this)
			return *this;

		delete[] website_;
		delete[] username_;
		delete[] password_;

		website_ = other.website_;
		username_ = other.username_;
		password_ = other.password_;

		other.website_ = nullptr;
		other.username_ = nullptr;
		other.password_ = nullptr;

		return *this;
	}

	~Entry()
	{
		delete[] website_;
		delete[] username_;
		delete[] password_;
	}
};

class Vault
{
private:
	mutable Vector<Entry> m_entries;
	mutable bool m_loaded = false;

	void writeTemp()
	{
		uint64_t totalSize = 0;
		uint32_t ut32Size = sizeof(uint32_t);

		//compute total serialized size (bytes). Could use uint32_t but 64_t helps prevent risk of overflow
		for (const auto& e : m_entries)
		{
			//reserve space for [size] [contents] for web, user, pass
			uint64_t websiteBytes = (ut32Size + (strlen(e.website_) + 1));
			uint64_t usernameBytes = (ut32Size + (strlen(e.username_) + 1));
			uint64_t passwordBytes = (ut32Size + (strlen(e.password_) + 1));

			totalSize += websiteBytes + usernameBytes + passwordBytes;
		}

		//allocate buffer of exactly totalSize
		Vector<uint8_t> buffer(totalSize);

		uint8_t* cursor = buffer.data();//ptr for advancing through buffer

		//[size][contents]
		for (const auto& e : m_entries)
		{
			auto writeToBuffer = [&](const char* s) -> void
				{
					//compute entry.x, +1 for null terminator
					uint32_t length = static_cast<uint32_t>((strlen(s) + 1));

					//[size] into buffer
					std::memcpy(cursor, &length, ut32Size);
					cursor += ut32Size;

					//[contents] into buffer
					std::memcpy(cursor, s, length);
					cursor += length;
				};

			//website
			writeToBuffer(e.website_);
			writeToBuffer(e.username_);
			writeToBuffer(e.password_);
		}

		//encrypted buffer
		Vector<uint8_t> cipher = Crypto::encryptData(buffer);

		//name, write as bytes | overwrite to end of file
		std::ofstream of("entries.bin", std::ios::binary);

		//write expects char. write entire buffer to entries.bin
		of.write(reinterpret_cast<const char*>(cipher.data()), cipher.size());
		of.close();

		m_loaded = true;
	}

	void readTemp(const char* fileName) const
	{
		//clear
		m_entries.clear();

		//avoid repeated calc
		uint32_t ut32Size = sizeof(uint32_t);
		std::size_t oneMBSize = 1024 * 1024;

		//read file in as binary data
		std::ifstream inFile(fileName, std::ios::binary);

		if (!inFile)
			throw std::runtime_error("File not found");

		//get file size in bytes
		uint64_t totalSize = std::filesystem::file_size(fileName);

		//init buffer
		Vector<uint8_t> buffer(totalSize);

		//read totalSize data into buffer. inFile expects chars, so cast
		inFile.read(reinterpret_cast<char*>(buffer.data()), totalSize);

		if (!inFile)
			throw std::runtime_error("Could not read file");

		//decryption
		Vector<uint8_t> plainBytes = Crypto::decryptData(buffer);

		//for decrypted container traversal
		uint8_t* end = plainBytes.data() + plainBytes.size();
		uint8_t* cursor = plainBytes.data();

		//clear and rebuild m_entries instead of temp Vector<Entry> and then moving
		m_entries.clear();

		//init length to store length for each entry, copy into it, advance ptr, temp storage for [contents], copy into, advance, return
		auto readFromBuffer = [&]()
			{
				//bounds check there is enough room left for [size]
				if (cursor + ut32Size > end)
					throw std::runtime_error("Insufficient remaining space");

				//move [length] into length and advance cursor to read [contents] next
				uint32_t length;
				std::memcpy(&length, cursor, ut32Size);
				cursor += ut32Size;

				//bounds check there is enough room left for [contents]
				if (cursor + length > end || length == 0 || length > oneMBSize)
					throw std::runtime_error("Insufficient remaining space for length OR length is 0");

				//char* entry = new char[length];
				UniquePtr<char[]> entry = makeUnique<char[]>(length);

				std::memcpy(entry.get(), cursor, length);
				cursor += length;

				return entry;
			};

		while (cursor < end)
		{
			UniquePtr<char[]> website = readFromBuffer();
			UniquePtr<char[]> username = readFromBuffer();
			UniquePtr<char[]> password = readFromBuffer();

			//construct directly into m_entries
			m_entries.emplace_back(website.get(), username.get(), password.get());
		}

		m_loaded = true;
	}

public:
	//disable copying because Entry cannot copy construct
	Vault(const Vault&) = delete;
	Vault& operator= (const Vault&) = delete;

	//default constructor
	Vault() = default;

	//move constructor
	Vault(Vault&& other) = default;

	//deep move assignment operator
	Vault& operator= (Vault&& other) = default;

	//destructor
	~Vault() = default;

	//user is intended to call this one, readTemp is for testing purposes and if the filename needs to be changed
	void readVault() const
	{
		if (m_loaded)
			return;

		readTemp("entries.bin");
	}

	void addEntryAndSave(const char* website, const char* username, const char* password)
	{
		//file exists, is regular file, and has some data in it
		if (std::filesystem::exists("entries.bin") && std::filesystem::is_regular_file("entries.bin") && std::filesystem::file_size("entries.bin") > 0)
			 readVault();

		//prevent duplicate entries
		for (const auto& e : m_entries)
		{
			if (std::strcmp(e.website_, website) == 0 && std::strcmp(e.username_, username) == 0 && std::strcmp(e.password_, password) == 0)
			{
				std::cout << "Duplicate entry, not appending\n";
				return;
			}
		}

		m_entries.emplace_back(website, username, password);

		writeTemp();
		listAllEntries();
	}

	//non const since Sort is called. Container is modified.
	void deleteEntryAndSave(Vector<std::size_t>& list)
	{
		if (list.size() < 1)
			throw std::runtime_error("Must have atleast one value to delete");

		//file exists, is regular file, and has some data in it
		if (std::filesystem::exists("entries.bin") && std::filesystem::is_regular_file("entries.bin") && std::filesystem::file_size("entries.bin") > 0)
			//load all Entry
			readVault();
		else
			throw std::runtime_error("Vault is empty or missing");

		//personal Sort
		Sort(list.data(), list.data() + list.size());

		//Make sure index is correct
		std::cout << "Delete Entry containing contents: \n";

		//make sure no duplicate indices, if there are, throw because user is silly
		for (std::size_t i{ 1 }; i < list.size(); ++i)
		{
			if (list[i] == list[i - 1])
				throw std::runtime_error("Duplicate index provided. That's really dumb.");
		}

		//preview
		for (std::size_t i{ 0 }; i < list.size(); ++i)
		{
			//[[[Entry[args[i]]. Get the Entry at args position, for each args starting from the 0th (because list is a stored as a Vector)
			//
			//[Entry1][Entry2][Entry3][Entry4][Entry5]
			//(0, 2)
			//i = 0			i < 2
			const Entry& e = m_entries[list[i]];
			std::cout << std::setw(4) << "[Index " << list[i] << " - " << "Website: "
				<< std::setw(12) << std::left << e.website_ << " | Username: "
				<< std::setw(12) << std::left << e.username_ << " | Password: "
				<< std::setw(12) << std::left << e.password_ << "]\n";
		}

		//confirm or deny
		for (;;)
		{
			std::cout << "Delete these entries? ";
			
			//enough for "yes\0" and "no\0"
			UniquePtr<char[]> response = makeUnique<char[]>(16);

			//consume all leading whitespace
			std::cin >> std::ws;

			//read up to 15 chars
			std::cin.getline(response.get(), 16);

			//cast all to lower
			for (char* p = response.get(); *p; ++p)
				*p = std::tolower(static_cast<unsigned char>(*p));

			if (strcmp(response.get(), "no") == 0 || strcmp(response.get(), "n") == 0)
				return; //exit
				

			if (strcmp(response.get(), "yes") == 0 || strcmp(response.get(), "y") == 0) 
				break; //exit for loop and continue downwards
				

			else
			{
				std::cout << "Invalid input, try again\n";
				continue;
			}

		}

		//erase specified Entrys at indices
		for (std::size_t i{ list.size() }; i > 0; --i)
			m_entries.erase_index(list[i - 1]);

		//write and save
		writeTemp();
		listAllEntries();

		m_loaded = true;
	}

	//Overload for variadic template. Delegates to Vector param version. Enable if to ensure only numbers passed.
	template <typename... Args, typename = std::enable_if_t<(std::is_integral<Args>::value && ...)>>
	void deleteEntryAndSave(Args... args)
	{
		static_assert(sizeof...(Args) > 0, "requires at least one index");

		//cast args to a size_t Vector for ease of use later
		Vector<std::size_t> list = { static_cast<std::size_t>(args)... };

		deleteEntryAndSave(list);
	}

	void editAndSave(std::size_t index, const char* newWebsite, const char* newUsername, const char* newPassword)
	{
		//retrieve all Entry into entries Vector
		readVault();

		//make sure in bounds index
		if (index >= m_entries.size())
			throw std::runtime_error("Out of bounds index");

		//move construct new Entry at index
		m_entries[index] = Entry(newWebsite, newUsername, newPassword);

		//write to file
		writeTemp();
		listAllEntries();
	}

	//overload for taking an Entry to add
	void editAndSave(std::size_t index, const Entry& et)
	{
		editAndSave(index, et.website_, et.username_, et.password_);
	}

	//just formatting for entries to look more even
	void listAllEntries() const
	{
		readVault();

		std::cout << "\n";

		for (std::size_t i{ 0 }; i < m_entries.size(); ++i)
			std::cout << std::setw(4) << "[Index " << i << " - " << "Website: "
			<< std::setw(16) << std::left << m_entries[i].website_ << " | Username: "
			<< std::setw(16) << std::left << m_entries[i].username_ << " | Password: "
			<< std::setw(16) << std::left << m_entries[i].password_ << "]\n";
		
		std::cout << "\n";
	}

	//non const getter
	Entry& get(std::size_t index)
	{
		readVault();

		if (index >= m_entries.size())
			throw std::runtime_error("Out of bounds index");
		return m_entries[index];
	}

	//const getter
	const Entry& get(std::size_t index) const
	{
		readVault();

		if (index >= m_entries.size())
			throw std::runtime_error("Out of bounds index");
		return m_entries[index];
	}

	void displayCmds()
	{
		std::cout << "\n";

		std::cout << "Commands: \n"
			<< "Display all entries: display\n"
			<< "Add an entry: add(website, username, password)\n"
			<< "Edit an entry: edit(index)\n"
			<< "Delete entries: delete(i,j,k...)\n\n";
	}
	


};
	
#endif