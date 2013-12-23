// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/webui/dom_distiller_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace dom_distiller {

DomDistillerHandler::DomDistillerHandler(DomDistillerService* service,
                                         const std::string& scheme)
    : service_(service), article_scheme_(scheme), weak_ptr_factory_(this) {}

DomDistillerHandler::~DomDistillerHandler() {}

void DomDistillerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestEntries",
      base::Bind(&DomDistillerHandler::HandleRequestEntries,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addArticle",
      base::Bind(&DomDistillerHandler::HandleAddArticle,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectArticle",
      base::Bind(&DomDistillerHandler::HandleSelectArticle,
                 base::Unretained(this)));
}

void DomDistillerHandler::HandleAddArticle(const base::ListValue* args) {
  std::string url;
  args->GetString(0, &url);
  GURL gurl(url);
  if (gurl.is_valid()) {
    service_->AddToList(
        gurl,
        base::Bind(base::Bind(&DomDistillerHandler::OnArticleAdded,
                              base::Unretained(this))));
  } else {
    web_ui()->CallJavascriptFunction("domDistiller.onArticleAddFailed");
  }
}

void DomDistillerHandler::HandleSelectArticle(const base::ListValue* args) {
  std::string entry_id;
  args->GetString(0, &entry_id);

  // TODO(nyquist): Do something here.
}

void DomDistillerHandler::HandleRequestEntries(const base::ListValue* args) {
  base::ListValue entries;
  const std::vector<ArticleEntry>& entries_specifics = service_->GetEntries();
  for (std::vector<ArticleEntry>::const_iterator it = entries_specifics.begin();
       it != entries_specifics.end();
       ++it) {
    const ArticleEntry& article = *it;
    DCHECK(IsEntryValid(article));
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("entry_id", article.entry_id());
    std::string title = (!article.has_title() || article.title().empty())
                            ? article.entry_id()
                            : article.title();
    entry->SetString("title", title);
    entries.Append(entry.release());
  }
  web_ui()->CallJavascriptFunction("domDistiller.onReceivedEntries", entries);
}

void DomDistillerHandler::OnArticleAdded(bool article_available) {
  // TODO(nyquist): Update this function.
  if (article_available) {
    HandleRequestEntries(NULL);
  } else {
    web_ui()->CallJavascriptFunction("domDistiller.onArticleAddFailed");
  }
}

}  // namespace dom_distiller
