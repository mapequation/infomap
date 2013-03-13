/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef FILEURI_H_
#define FILEURI_H_
#include <iostream>
#include <string>
using std::string;

/**
 * Filename class to simplify handling of parts of a filename.
 * If a path is path/to/file.ext, the member methods of this class give these parts:
 * getFilename -> "path/to/file.ext"
 * getDirectory -> "path/to/"
 * getName -> "file"
 * getExtension -> "ext"
 */
class FileURI
{
public:
	FileURI(); // Good to be able to have a Filename member in a class without require initialization in initialization list
	FileURI(const char* filename, bool requireExtension = false);
	FileURI(const string& filename, bool requireExtension = false);
	FileURI(const FileURI& other);
	FileURI & operator= (const FileURI& other);

	const string& getFilename() const
    {
        return m_filename;
    }

	/**
	 * Includes last '/' if non-empty.
	 */
    const string& getDirectory() const
    {
        return m_directory;
    }

    const string& getName() const
    {
        return m_name;
    }

    const string& getExtension() const
    {
        return m_extension;
    }

    std::string getParts()
	{
		std::string out("['" +
				getDirectory() + "' + '" +
				getName() + "' + '" +
				getExtension() + "']");
		return out;
	}

	friend std::ostream& operator<<(std::ostream& out, const FileURI& file)
	{
		return out << file.getFilename();
	}

private:
    void analyzeFilename();
    string getErrorMessage();

    string m_filename;
    bool m_requireExtension;

	string m_directory;
	string m_name;
	string m_extension;
};

#endif /* FILEURI_H_ */
