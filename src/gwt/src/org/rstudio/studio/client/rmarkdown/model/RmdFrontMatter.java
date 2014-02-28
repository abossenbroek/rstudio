/*
 * RmdFrontMatter.java
 *
 * Copyright (C) 2009-14 by RStudio, Inc.
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
package org.rstudio.studio.client.rmarkdown.model;

import com.google.gwt.core.client.JavaScriptObject;
import com.google.gwt.core.client.JsArrayString;

public class RmdFrontMatter extends JavaScriptObject
{
   protected RmdFrontMatter()
   {
   }
   
   public final native JsArrayString getFormatList() /*-{
     return Object.getOwnPropertyNames(this.output);
   }-*/;

   public final native RmdFrontMatterOutputOptions getOutputOption(
         String format) /*-{
     return this.output[format] || null;
   }-*/;
   
   public final native void setOutputOption(String format, 
         JavaScriptObject options) /*-{
     this.output[format] = options;
   }-*/;
   
   public final static String FRONTMATTER_SEPARATOR = "---\n";
}