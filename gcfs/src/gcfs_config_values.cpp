#include "gcfs_config_values.h"

#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <gcfs.h>
#include <gcfs_config.h>
#include <gcfs_service.h>
#include <gcfs_utils.h>

GCFS_ConfigValue::GCFS_ConfigValue(GCFS_Directory * pParent):
   GCFS_FileSystem(pParent),
   m_iSize(0),
   m_bIsSet(false)
{
   
}

GCFS_ConfigValue::~GCFS_ConfigValue()
{
   
}

ssize_t GCFS_ConfigValue::read(std::string &sBuffer, off_t uiOffset, size_t uiSize)
{
   // If default value found, use it
   GCFS_ConfigValue* pValue = getValidValue();
   if(pValue == NULL)
   {
      printf("Default value %s not found!", getName());
      pValue = this;
   }
   
   pValue->PrintValue(sBuffer);
   sBuffer += "\n";
   return max(0, sBuffer.size() - uiOffset);
}

ssize_t GCFS_ConfigValue::write(const char* sBuffer, off_t uiOffset, size_t uiSize)
{
   // If value not yet set - copy from parent
   GCFS_ConfigValue* pValue;
   if(!m_bIsSet && (pValue = getValidValue()) != NULL)
   {
      copyValue(pValue);
   }

   std::string sValue = GCFS_Utils::TrimStr(std::string(sBuffer, uiSize));
   
   SetValue(sValue.c_str(), uiOffset);

   // Indicate value is valid
   m_bIsSet = true;

   // Update config file size
   m_iSize = sValue.length();
   
   return uiSize;
}

bool GCFS_ConfigValue::getPermissions(GCFS_Permissions& sPermissions)
{
   if(GCFS_FileSystem::getPermissions(sPermissions))
   {
      mode_t sExecuteFlags = 0111;

      if(m_bIsSet)
         sPermissions.m_sMode |= sExecuteFlags;
      else
         sPermissions.m_sMode &= ~sExecuteFlags;
      
      return true;
   }

   return false;
}

bool GCFS_ConfigValue::copyValue(GCFS_ConfigValue* pFrom)
{
   if(pFrom)
   {
      std::string sValue;
      pFrom->PrintValue(sValue);
      SetValue(sValue.c_str());
      return true;
   }

   return false;
}

GCFS_ConfigValue* GCFS_ConfigValue::getValidValue()
{
   if(m_bIsSet)
      return this;

   GCFS_Task * pTask = getParentTask();
   const char * sName = getName();

   if(sName)
   {
      while((pTask = pTask->getParentTask()) != NULL)
      {
         GCFS_ConfigValue* pConfig = pTask->getConfigValue(sName);

         if(!pConfig) // Should not happen
            return NULL;

         if(pConfig->m_bIsSet)
            return pConfig;
      }

      // No value set, use default from config
      GCFS_Service* pService = g_sConfig.GetService(getParentTask()->m_sConfigDirectory.m_iService);
      if(pService)
         return pService->m_sDefaultValues.getConfigValue(sName);
   }

   return NULL;
}

/***************************************************************************/
GCFS_ConfigInt::GCFS_ConfigInt(GCFS_Directory * pParent, const char *sDefault) :GCFS_ConfigValue(pParent)
{
   this->SetValue(sDefault);
}

bool GCFS_ConfigInt::SetValue(const char* sValue, size_t iOffset)
{
   m_iValue = atoi(sValue);

   return true;
}

bool GCFS_ConfigInt::PrintValue(std::string& buff)
{
   size_t size = snprintf(NULL, 0, "%d", m_iValue);
   buff.resize(size+1);
   snprintf((char*)buff.c_str(), size+1, "%d", m_iValue);

   return true;
}

GCFS_ConfigInt::operator int()
{
   return m_iValue;
}

/***************************************************************************/
GCFS_ConfigString::GCFS_ConfigString(GCFS_Directory * pParent, const char *sDefault): GCFS_ConfigValue(pParent)
{
   this->SetValue(sDefault);

}

bool GCFS_ConfigString::SetValue(const char* sValue, size_t iOffset)
{
   m_sValue = sValue;

   return true;
}

bool GCFS_ConfigString::PrintValue(std::string& buff)
{
   buff = m_sValue;

   return true;
}

GCFS_ConfigString::operator std::string()
{
   return m_sValue;
}

/***************************************************************************/
GCFS_ConfigChoice::GCFS_ConfigChoice(GCFS_Directory * pParent, const char *sDefault, choices_t * pvChoices) :GCFS_ConfigValue(pParent)
{
   if(pvChoices)
      m_vChoices = *pvChoices;

   if(sDefault)
      this->SetValue(sDefault);
   else
      m_iValue = 0;
}

bool GCFS_ConfigChoice::SetValue(const char* sValue, size_t iOffset)
{
   if(!sValue)
      return false;

   int iVal = -1;
   for(int iIndex = 0; iIndex < (int)m_vChoices.size(); iIndex ++)
      if(strcasecmp(m_vChoices[iIndex].c_str(), sValue) == 0)
         iVal = iIndex;

   if(iVal < 0)
      return false;

   m_iValue = iVal;

   return true;
}

bool GCFS_ConfigChoice::PrintValue(std::string& buff)
{
   for(int iIndex = 0; iIndex < (int)m_vChoices.size(); iIndex ++)
   {
      if(iIndex > 0)
         buff += ", ";

      if(iIndex == m_iValue)
         buff +=  "[";

      buff += m_vChoices[iIndex];

      if(iIndex == m_iValue)
         buff +=  "]";
   }

   return true;
}

GCFS_ConfigChoice::operator int()
{
   return m_iValue;
}

/***************************************************************************/
GCFS_ConfigEnvironment::GCFS_ConfigEnvironment(GCFS_Directory * pParent, const char* sDefault): GCFS_ConfigValue(pParent)
{
   this->SetValue(sDefault);
}

bool GCFS_ConfigEnvironment::SetValue(const char* sValue, size_t iOffset)
{
   if(!sValue)
      return false;

   if(!iOffset) // Allow overwrite
      m_mValues.clear();
   else if(iOffset == (size_t)-1 || iOffset == m_iSize) // Allow to append
      ;
   else // Dont allow to write in the middle
      return false;

   wordexp_t sArguments;
   if(wordexp(sValue, &sArguments, WRDE_SHOWERR))
      return false;

   for (uint iIndex = 0; iIndex < sArguments.we_wordc; iIndex++)
   {
      // Separate key=value string into two pieces
      char* pcSeparator = strchr(sArguments.we_wordv[iIndex], '=');
      *pcSeparator='\0';

      printf("Argument: %s, tokens: %s, %s\n", sArguments.we_wordv[iIndex], sArguments.we_wordv[iIndex], pcSeparator+1);

      m_mValues[sArguments.we_wordv[iIndex]] = pcSeparator+1;
   }
   wordfree(&sArguments);

   return true;
}

bool GCFS_ConfigEnvironment::SetValue(const char* sKey, const char* sValue)
{
   if(!sValue || !sKey)
      return false;

   m_mValues[sKey]=sValue;

   return true;
}

bool GCFS_ConfigEnvironment::PrintValue(std::string& buff)
{
   values_t::iterator it;
   for(it = m_mValues.begin(); it != m_mValues.end(); it++)
      buff += it->first+"="+it->second+"\n";

   return true;
}

GCFS_ConfigEnvironment::operator values_t&()
{
   return m_mValues;
}

/***************************************************************************/
GCFS_ConfigService::GCFS_ConfigService(GCFS_Directory * pParent, const char* sDefault, GCFS_ConfigChoice::choices_t* pvChoices)
   :GCFS_ConfigChoice(pParent, sDefault, pvChoices)
{
   this->SetValue(sDefault);

}

bool GCFS_ConfigService::SetValue(const char* sValue, size_t iOffset)
{
   bool bRet = GCFS_ConfigChoice::SetValue(sValue, iOffset);

   if(!bRet)
      return bRet;

   if(m_iValue >= 0)
      g_sConfig.GetService(m_iValue)->customizeTask(getParentTask());

   return true;
}

