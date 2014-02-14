// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace omnibox = api::omnibox;
namespace SendSuggestions = omnibox::SendSuggestions;
namespace SetDefaultSuggestion = omnibox::SetDefaultSuggestion;

namespace {

const char kSuggestionContent[] = "content";
const char kCurrentTabDisposition[] = "currentTab";
const char kForegroundTabDisposition[] = "newForegroundTab";
const char kBackgroundTabDisposition[] = "newBackgroundTab";

// Pref key for omnibox.setDefaultSuggestion.
const char kOmniboxDefaultSuggestion[] = "omnibox_default_suggestion";

#if defined(OS_LINUX)
static const int kOmniboxIconPaddingLeft = 2;
static const int kOmniboxIconPaddingRight = 2;
#elif defined(OS_MACOSX)
static const int kOmniboxIconPaddingLeft = 0;
static const int kOmniboxIconPaddingRight = 2;
#else
static const int kOmniboxIconPaddingLeft = 0;
static const int kOmniboxIconPaddingRight = 0;
#endif

scoped_ptr<omnibox::SuggestResult> GetOmniboxDefaultSuggestion(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionPrefs* prefs =
      ExtensionSystem::Get(profile)->extension_service()->extension_prefs();

  scoped_ptr<omnibox::SuggestResult> suggestion;
  const base::DictionaryValue* dict = NULL;
  if (prefs && prefs->ReadPrefAsDictionary(extension_id,
                                           kOmniboxDefaultSuggestion,
                                           &dict)) {
    suggestion.reset(new omnibox::SuggestResult);
    omnibox::SuggestResult::Populate(*dict, suggestion.get());
  }
  return suggestion.Pass();
}

// Tries to set the omnibox default suggestion; returns true on success or
// false on failure.
bool SetOmniboxDefaultSuggestion(
    Profile* profile,
    const std::string& extension_id,
    const omnibox::DefaultSuggestResult& suggestion) {
  ExtensionPrefs* prefs =
      ExtensionSystem::Get(profile)->extension_service()->extension_prefs();
  if (!prefs)
    return false;

  scoped_ptr<base::DictionaryValue> dict = suggestion.ToValue();
  // Add the content field so that the dictionary can be used to populate an
  // omnibox::SuggestResult.
  dict->SetWithoutPathExpansion(kSuggestionContent, new base::StringValue(""));
  prefs->UpdateExtensionPref(extension_id,
                             kOmniboxDefaultSuggestion,
                             dict.release());

  return true;
}

}  // namespace

// static
void ExtensionOmniboxEventRouter::OnInputStarted(
    Profile* profile, const std::string& extension_id) {
  scoped_ptr<Event> event(new Event(
      omnibox::OnInputStarted::kEventName,
      make_scoped_ptr(new base::ListValue())));
  event->restrict_to_browser_context = profile;
  ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

// static
bool ExtensionOmniboxEventRouter::OnInputChanged(
    Profile* profile, const std::string& extension_id,
    const std::string& input, int suggest_id) {
  if (!extensions::ExtensionSystem::Get(profile)->event_router()->
          ExtensionHasEventListener(extension_id,
                                    omnibox::OnInputChanged::kEventName))
    return false;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue(input));
  args->Set(1, new base::FundamentalValue(suggest_id));

  scoped_ptr<Event> event(new Event(omnibox::OnInputChanged::kEventName,
                                    args.Pass()));
  event->restrict_to_browser_context = profile;
  ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
  return true;
}

// static
void ExtensionOmniboxEventRouter::OnInputEntered(
    content::WebContents* web_contents,
    const std::string& extension_id,
    const std::string& input,
    WindowOpenDisposition disposition) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  const Extension* extension =
      ExtensionSystem::Get(profile)->extension_service()->extensions()->
          GetByID(extension_id);
  CHECK(extension);
  extensions::TabHelper::FromWebContents(web_contents)->
      active_tab_permission_granter()->GrantIfRequested(extension);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue(input));
  if (disposition == NEW_FOREGROUND_TAB)
    args->Set(1, new base::StringValue(kForegroundTabDisposition));
  else if (disposition == NEW_BACKGROUND_TAB)
    args->Set(1, new base::StringValue(kBackgroundTabDisposition));
  else
    args->Set(1, new base::StringValue(kCurrentTabDisposition));

  scoped_ptr<Event> event(new Event(omnibox::OnInputEntered::kEventName,
                                    args.Pass()));
  event->restrict_to_browser_context = profile;
  ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_OMNIBOX_INPUT_ENTERED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());
}

// static
void ExtensionOmniboxEventRouter::OnInputCancelled(
    Profile* profile, const std::string& extension_id) {
  scoped_ptr<Event> event(new Event(
      omnibox::OnInputCancelled::kEventName,
      make_scoped_ptr(new base::ListValue())));
  event->restrict_to_browser_context = profile;
  ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

OmniboxAPI::OmniboxAPI(Profile* profile)
    : profile_(profile),
      url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
  if (url_service_) {
    template_url_sub_ = url_service_->RegisterOnLoadedCallback(
        base::Bind(&OmniboxAPI::OnTemplateURLsLoaded,
                   base::Unretained(this)));
  }

  // Use monochrome icons for Omnibox icons.
  omnibox_popup_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_padding(gfx::Insets(0, kOmniboxIconPaddingLeft,
                                                0, kOmniboxIconPaddingRight));
}

void OmniboxAPI::Shutdown() {
  template_url_sub_.reset();
}

OmniboxAPI::~OmniboxAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<OmniboxAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<OmniboxAPI>* OmniboxAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
OmniboxAPI* OmniboxAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<OmniboxAPI>::GetForProfile(profile);
}

void OmniboxAPI::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const Extension* extension =
        content::Details<const Extension>(details).ptr();
    const std::string& keyword = OmniboxInfo::GetKeyword(extension);
    if (!keyword.empty()) {
      // Load the omnibox icon so it will be ready to display in the URL bar.
      omnibox_popup_icon_manager_.LoadIcon(profile_, extension);
      omnibox_icon_manager_.LoadIcon(profile_, extension);

      if (url_service_) {
        url_service_->Load();
        if (url_service_->loaded()) {
          url_service_->RegisterOmniboxKeyword(extension->id(),
                                               extension->name(),
                                               keyword);
        } else {
          pending_extensions_.insert(extension);
        }
      }
    }
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<UnloadedExtensionInfo>(details)->extension;
    if (!OmniboxInfo::GetKeyword(extension).empty()) {
      if (url_service_) {
        if (url_service_->loaded())
          url_service_->UnregisterOmniboxKeyword(extension->id());
        else
          pending_extensions_.erase(extension);
      }
    }
  } else {
    NOTREACHED();
  }
}

gfx::Image OmniboxAPI::GetOmniboxIcon(const std::string& extension_id) {
  return gfx::Image::CreateFrom1xBitmap(
      omnibox_icon_manager_.GetIcon(extension_id));
}

gfx::Image OmniboxAPI::GetOmniboxPopupIcon(const std::string& extension_id) {
  return gfx::Image::CreateFrom1xBitmap(
      omnibox_popup_icon_manager_.GetIcon(extension_id));
}

void OmniboxAPI::OnTemplateURLsLoaded() {
  // Register keywords for pending extensions.
  template_url_sub_.reset();
  for (PendingExtensions::const_iterator i(pending_extensions_.begin());
       i != pending_extensions_.end(); ++i) {
    url_service_->RegisterOmniboxKeyword((*i)->id(),
                                         (*i)->name(),
                                         OmniboxInfo::GetKeyword(*i));
  }
  pending_extensions_.clear();
}

template <>
void ProfileKeyedAPIFactory<OmniboxAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

bool OmniboxSendSuggestionsFunction::RunImpl() {
  scoped_ptr<SendSuggestions::Params> params(
      SendSuggestions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_OMNIBOX_SUGGESTIONS_READY,
      content::Source<Profile>(GetProfile()->GetOriginalProfile()),
      content::Details<SendSuggestions::Params>(params.get()));

  return true;
}

bool OmniboxSetDefaultSuggestionFunction::RunImpl() {
  scoped_ptr<SetDefaultSuggestion::Params> params(
      SetDefaultSuggestion::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (SetOmniboxDefaultSuggestion(
          GetProfile(), extension_id(), params->suggestion)) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,
        content::Source<Profile>(GetProfile()->GetOriginalProfile()),
        content::NotificationService::NoDetails());
  }

  return true;
}

// This function converts style information populated by the JSON schema
// compiler into an ACMatchClassifications object.
ACMatchClassifications StyleTypesToACMatchClassifications(
    const omnibox::SuggestResult &suggestion) {
  ACMatchClassifications match_classifications;
  if (suggestion.description_styles) {
    base::string16 description = base::UTF8ToUTF16(suggestion.description);
    std::vector<int> styles(description.length(), 0);

    for (std::vector<linked_ptr<omnibox::SuggestResult::DescriptionStylesType> >
         ::iterator i = suggestion.description_styles->begin();
         i != suggestion.description_styles->end(); ++i) {
      omnibox::SuggestResult::DescriptionStylesType* style = i->get();

      int length = description.length();
      if (style->length)
        length = *style->length;

      size_t offset = style->offset >= 0 ? style->offset :
          std::max(0, static_cast<int>(description.length()) + style->offset);

      int type_class;
      switch (style->type) {
        case omnibox::SuggestResult::DescriptionStylesType::TYPE_URL:
          type_class = AutocompleteMatch::ACMatchClassification::URL;
          break;
        case omnibox::SuggestResult::DescriptionStylesType::TYPE_MATCH:
          type_class = AutocompleteMatch::ACMatchClassification::MATCH;
          break;
        case omnibox::SuggestResult::DescriptionStylesType::TYPE_DIM:
          type_class = AutocompleteMatch::ACMatchClassification::DIM;
          break;
        default:
          type_class = AutocompleteMatch::ACMatchClassification::NONE;
          return match_classifications;
      }

      for (size_t j = offset; j < offset + length && j < styles.size(); ++j)
        styles[j] |= type_class;
    }

    for (size_t i = 0; i < styles.size(); ++i) {
      if (i == 0 || styles[i] != styles[i-1])
        match_classifications.push_back(
            ACMatchClassification(i, styles[i]));
    }
  } else {
    match_classifications.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }

  return match_classifications;
}

void ApplyDefaultSuggestionForExtensionKeyword(
    Profile* profile,
    const TemplateURL* keyword,
    const base::string16& remaining_input,
    AutocompleteMatch* match) {
  DCHECK(keyword->GetType() == TemplateURL::OMNIBOX_API_EXTENSION);

  scoped_ptr<omnibox::SuggestResult> suggestion(
      GetOmniboxDefaultSuggestion(profile, keyword->GetExtensionId()));
  if (!suggestion || suggestion->description.empty())
    return;  // fall back to the universal default

  const base::string16 kPlaceholderText(base::ASCIIToUTF16("%s"));
  const base::string16 kReplacementText(base::ASCIIToUTF16("<input>"));

  base::string16 description = base::UTF8ToUTF16(suggestion->description);
  ACMatchClassifications& description_styles = match->contents_class;
  description_styles = StyleTypesToACMatchClassifications(*suggestion);

  // Replace "%s" with the user's input and adjust the style offsets to the
  // new length of the description.
  size_t placeholder(description.find(kPlaceholderText, 0));
  if (placeholder != base::string16::npos) {
    base::string16 replacement =
        remaining_input.empty() ? kReplacementText : remaining_input;
    description.replace(placeholder, kPlaceholderText.length(), replacement);

    for (size_t i = 0; i < description_styles.size(); ++i) {
      if (description_styles[i].offset > placeholder)
        description_styles[i].offset += replacement.length() - 2;
    }
  }

  match->contents.assign(description);
}

}  // namespace extensions
