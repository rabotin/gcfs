#ifndef __GCFS_CONFIG_VALUES__
#define __GCFS_CONFIG_VALUES__

#include <stdlib.h>
#include <string>
#include <vector>
#include <map>

#include "gcfs_filesystem.h"

// Generic configuration values
class GCFS_Task;

class GCFS_ConfigValue : public GCFS_FileSystem
{
public:
                              GCFS_ConfigValue(GCFS_Directory * pParent);
   virtual                   ~GCFS_ConfigValue();

public:
    virtual ssize_t           read(std::string& sBuffer, off_t uiOffset, size_t uiSize);
    virtual ssize_t           write(const char* sBuffer, off_t uiOffset, size_t uiSize);
    virtual bool              getPermissions(GCFS_Permissions& sPermissions);
    virtual bool              copyValue(GCFS_ConfigValue* pFrom);
    virtual off_t             getSize();
    
public:
   virtual  bool              SetValue(const char * sValue, size_t iOffset = 0) = 0;
   virtual  bool              PrintValue(std::string &buff) = 0;
   virtual  GCFS_ConfigValue* getValidValue();

public:
   size_t         m_iSize;
   bool           m_bIsSet;
};

// Specific configuration values
/***************************************************************************/
class GCFS_ConfigInt: public GCFS_ConfigValue
{
public:
                  GCFS_ConfigInt(GCFS_Directory * pParent, const char *sDefault);

   virtual bool   SetValue(const char * sValue, size_t iOffset = 0);
   virtual bool   PrintValue(std::string &buff);
   int            get();

protected:
   int            m_iValue;
};

/***************************************************************************/
class GCFS_ConfigString: public GCFS_ConfigValue
{
public:
                  GCFS_ConfigString(GCFS_Directory * pParent, const char *sDefault);

   virtual bool   SetValue(const char * sValue, size_t iOffset = 0);
   virtual bool   PrintValue(std::string &buff);
   const char *   get();
   std::string&   getString();

protected:
   std::string    m_sValue;
};

/***************************************************************************/
class GCFS_ConfigChoice: public GCFS_ConfigValue
{
public:
   typedef std::vector<std::string> choices_t;

public:
                        GCFS_ConfigChoice(GCFS_Directory * pParent, const char* sDefault = NULL, GCFS_ConfigChoice::choices_t* pvChoices = NULL);

   virtual bool         SetValue(const char * sValue, size_t iOffset = 0);
   virtual bool         PrintValue(std::string &buff);
   int                  get();

protected:
   int                  m_iValue;
   choices_t            m_vChoices;
   virtual choices_t&   getChoices();
};

/***************************************************************************/
class GCFS_ConfigEnvironment: public GCFS_ConfigValue
{
public:
               typedef std::map<std::string, std::string> values_t;

public:
                        GCFS_ConfigEnvironment(GCFS_Directory * pParent, const char* sDefault = NULL);

   virtual bool         SetValue(const char * sValue, size_t iOffset = 0);
   bool                 SetValue(const char* sKey, const char* sValue);
   virtual bool         PrintValue(std::string &buff);
   values_t&            get();

protected:
   values_t             m_mValues;
};

/***************************************************************************/
class GCFS_ConfigService: public GCFS_ConfigChoice
{
public:
                        GCFS_ConfigService(GCFS_Directory * pParent, const char* sDefault = NULL, GCFS_ConfigChoice::choices_t* pvChoices = NULL);

public:
   virtual bool         SetValue(const char * sValue, size_t iOffset = 0);

protected:
   virtual choices_t&   getChoices();
};

#endif /*__GCFS_CONFIG_VALUES__*/
