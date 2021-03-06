/*
 * RSessionContext.cpp
 *
 * Copyright (C) 2009-12 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include <core/r_util/RSessionContext.hpp>

#include <iostream>

#include <boost/format.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <core/FilePath.hpp>
#include <core/Settings.hpp>
#include <core/FileSerializer.hpp>

#include <core/http/Util.hpp>
#include <core/http/URL.hpp>

#include <core/system/System.hpp>
#include <core/system/Environment.hpp>

#include <core/r_util/RActiveSessions.hpp>
#include <core/r_util/RProjectFile.hpp>

#define kSessionSuffix "-d"
#define kProjectNone   "none"

namespace rstudio {
namespace core {
namespace r_util {

SessionScope SessionScope::fromProject(
                               std::string project,
                               const std::string& id,
                               const FilePathToProjectId& filePathToProjectId)
{
   if (project != kProjectNone)
   {
      project = filePathToProjectId(project);
      return SessionScope(project, id);
   }
   else
   {
      return projectNone(id);
   }
}


std::string SessionScope::projectPathForScope(
                               const SessionScope& scope,
                               const ProjectIdToFilePath& projectIdToFilePath)
{
   return projectIdToFilePath(scope.project());
}


SessionScope SessionScope::fromProjectId(const std::string& project,
                                         const std::string& id)
{
   return SessionScope(project, id);
}

SessionScope SessionScope::projectNone(const std::string& id)
{
   return SessionScope(kProjectNoneId, id);
}

bool SessionScope::isProjectNone() const
{
   return project() == kProjectNoneId;
}

bool validateSessionScopeId(const FilePath& userScratchPath,
                            const std::string& id)
{
   r_util::ActiveSessions activeSessions(userScratchPath);
   boost::shared_ptr<r_util::ActiveSession> pSession = activeSessions.get(id);
   return pSession->hasRequiredProperties();
}

bool validateProjectSessionScope(
           const SessionScope& scope,
           const core::FilePath& userHomePath,
           const core::FilePath& userScratchPath,
           core::r_util::ProjectIdToFilePath projectIdToFilePath,
           std::string* pProjectFilePath)
{
   // lookup the project path by id
   std::string project = r_util::SessionScope::projectPathForScope(
                                   scope,
                                   projectIdToFilePath);
   if (!project.empty())
   {
      FilePath projectDir = FilePath::resolveAliasedPath(project, userHomePath);
      if (projectDir.exists())
      {
         FilePath projectPath = r_util::projectFromDirectory(projectDir);

         if (projectPath.exists())
         {
            if (validateSessionScopeId(userScratchPath, scope.id()))
            {
               *pProjectFilePath = projectPath.absolutePath();
               return true;
            }
         }
      }
   }

   // didn't succeed in validating the path
   return false;
}

std::string urlPathForSessionScope(const SessionScope& scope)
{
   // get a URL compatible project path
   std::string project = http::util::urlEncode(scope.project());
   boost::algorithm::replace_all(project, "%2F", "/");

   // create url
   boost::format fmt("/s/%1%%2%/");
   return boost::str(fmt % project % scope.id());
}

void parseSessionUrl(const std::string& url,
                     SessionScope* pScope,
                     std::string* pUrlPrefix,
                     std::string* pUrlWithoutPrefix)
{
   static boost::regex re("/s/([A-Fa-f0-9]{8})([A-Fa-f0-9]{8})/");

   boost::smatch match;
   if (boost::regex_search(url, match, re))
   {
      if (pScope)
      {
         std::string project = http::util::urlDecode(match[1]);
         std::string id = match[2];
         *pScope = r_util::SessionScope::fromProjectId(project, id);
      }
      if (pUrlPrefix)
      {
         *pUrlPrefix = match[0];
      }
      if (pUrlWithoutPrefix)
      {
         *pUrlWithoutPrefix = boost::algorithm::replace_first_copy(
                                   url, std::string(match[0]), "/");
      }
   }
   else
   {
      if (pScope)
         *pScope = SessionScope();
      if (pUrlPrefix)
         *pUrlPrefix = std::string();
      if (pUrlWithoutPrefix)
         *pUrlWithoutPrefix = url;
   }
}


std::string createSessionUrl(const std::string& hostPageUrl,
                             const SessionScope& scope)
{
   // get url without prefix
   std::string url;
   parseSessionUrl(hostPageUrl, NULL, NULL, &url);

   // build path for project
   std::string path = urlPathForSessionScope(scope);

   // complete the url and return it
   return http::URL::complete(url, path);
}


std::ostream& operator<< (std::ostream& os, const SessionContext& context)
{
   os << context.username;
   if (!context.scope.project().empty())
      os << " -- " << context.scope.project();
   if (!context.scope.id().empty())
      os << " [" << context.scope.id() << "]";
   return os;
}


std::string sessionScopeFile(std::string prefix,
                             const SessionScope& scope)
{   
   // resolve project path
   std::string project = scope.project();
   if (!project.empty())
   {
      // pluralize in the presence of project context so there
      // is no conflict when switching between single and multi-session
      if (!scope.project().empty())
         prefix += "s";

      if (!boost::algorithm::starts_with(project, "/"))
         project = "/" + project;

      if (!scope.id().empty())
      {
         if (!boost::algorithm::ends_with(project, "/"))
            project = project + "/";
      }
   }

   // return file path
   return prefix + project + scope.id();
}

std::string sessionScopePrefix(const std::string& username)
{
   return username + kSessionSuffix;
}

std::string sessionScopesPrefix(const std::string& username)
{
   // pluralize the prefix so there is no conflict when switching
   // between the single file and directory based schemas
   return username + kSessionSuffix "s";
}

std::string sessionContextFile(const SessionContext& context)
{
   return sessionScopeFile(sessionScopePrefix(context.username), context.scope);
}

std::string generateScopeId()
{
   std::vector<std::string> reserved;
   reserved.push_back(kProjectNoneId);
   reserved.push_back(kWorkspacesId);
   return generateScopeId(reserved);
}

std::string generateScopeId(const std::vector<std::string>& reserved)
{
   // generate id
   std::string id = core::string_utils::toLower(
                                 core::system::generateShortenedUuid());

   // ensure 8 chracters
   const size_t kLen = 8;
   if (id.length() != kLen)
   {
      if (id.length() > kLen)
      {
         id = id.substr(0, kLen);
      }
      else
      {
         size_t diff = kLen - id.length();
         std::string pad(diff, 'f');
         id += pad;
      }
   }

   // try again if this id is reserved
   if (std::find(reserved.begin(), reserved.end(), id) != reserved.end())
      return generateScopeId(reserved);
   else
      return id;
}

} // namespace r_util
} // namespace core 
} // namespace rstudio



