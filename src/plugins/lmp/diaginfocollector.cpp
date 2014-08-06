/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "diaginfocollector.h"
#include <gst/gst.h>
#include <taglib/taglib.h>

namespace LeechCraft
{
namespace LMP
{
	DiagInfoCollector::DiagInfoCollector ()
	{
		Strs_ << QString { "Built with GStreamer %1.%2.%3; running with %4" }
				.arg (GST_VERSION_MAJOR)
				.arg (GST_VERSION_MINOR)
				.arg (GST_VERSION_MICRO)
				.arg (QString::fromUtf8 (gst_version_string ()));
#ifdef WITH_LIBGUESS
		Strs_ << "Built WITH libguess";
#else
		Strs_ << "Built WITHOUT libguess";
#endif
		Strs_ << QString { "Built with Taglib %1.%2.%3" }
				.arg (TAGLIB_MAJOR_VERSION)
				.arg (TAGLIB_MINOR_VERSION)
				.arg (TAGLIB_PATCH_VERSION);
		Strs_ << "GStreamer plugins:";

		const auto plugins = gst_default_registry_get_plugin_list ();
		auto node = plugins;
		QStringList pluginsList;
		while (node)
		{
			const auto plugin = static_cast<GstPlugin*> (node->data);
			pluginsList << QString { "* %1 (from %2)" }
					.arg (QString::fromUtf8 (plugin->desc.name))
					.arg (QString::fromUtf8 (plugin->filename));

			node = g_list_next (node);
		}

		pluginsList.sort ();

		Strs_ += pluginsList;

		gst_plugin_list_free (plugins);
	}

	QString DiagInfoCollector::operator() () const
	{
		return Strs_.join ("\n");
	}
}
}
