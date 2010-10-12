// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ie_toolbar_import_win.h"

#include "base/basictypes.h"
#include "base/registry.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/util/data_encryption.h"

// Forward declaration. This function is not in unnamed namespace as it
// is referenced in the unittest.
bool ImportCurrentUserProfiles(std::vector<AutoFillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards);
namespace {

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/autofill/internal/autofill_ie_toolbar_decryption.h"
#else  // defined(GOOGLE_CHROME_BUILD)
inline std::wstring DecryptCCNumber(const std::wstring& data) {
  return std::wstring();
}
inline bool IsEmptySalt(const std::wstring& salt) {
  return false;
}
#endif  // defined(GOOGLE_CHROME_BUILD)

const wchar_t* const kProfileKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Profiles";
const wchar_t* const kCreditCardKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Credit Cards";
const wchar_t* const kPasswordHashValue = L"password_hash";
const wchar_t* const kSaltValue = L"salt";

string16 ReadAndDecryptValue(RegKey* key, const wchar_t* value_name) {
  DWORD data_type = REG_BINARY;
  DWORD data_size = 0;
  if (!key->ReadValue(value_name, NULL, &data_size, &data_type) ||
      !data_size || data_type != REG_BINARY)
    return string16();
  std::vector<uint8> data;
  data.resize(data_size);
  if (key->ReadValue(value_name, &(data[0]), &data_size, &data_type)) {
    std::string out_data;
    if (DecryptData(data, &out_data)) {
      // The actual data is in UTF16 already.
      if (!(out_data.size() & 1) && (out_data.size() > 2) &&
          !out_data[out_data.size() - 1] && !out_data[out_data.size() - 2]) {
        return string16(
            reinterpret_cast<const wchar_t *>(out_data.c_str()));
      }
    }
  }
  return string16();
}

struct {
  AutoFillFieldType field_type;
  const wchar_t *reg_value_name;
} profile_reg_values[] = {
  { NAME_FIRST,                    L"name_first" },
  { NAME_MIDDLE,                   L"name_middle" },
  { NAME_LAST,                     L"name_last" },
  { NAME_SUFFIX,                   L"name_suffix" },
  { EMAIL_ADDRESS,                 L"email" },
  { COMPANY_NAME,                  L"company_name" },
  { PHONE_HOME_NUMBER,             L"phone_home_number" },
  { PHONE_HOME_CITY_CODE,          L"phone_home_city_code" },
  { PHONE_HOME_COUNTRY_CODE,       L"phone_home_country_code" },
  { PHONE_FAX_NUMBER,              L"phone_fax_number" },
  { PHONE_FAX_CITY_CODE,           L"phone_fax_city_code" },
  { PHONE_FAX_COUNTRY_CODE,        L"phone_fax_country_code" },
  { ADDRESS_HOME_LINE1,            L"address_home_line1" },
  { ADDRESS_HOME_LINE2,            L"address_home_line2" },
  { ADDRESS_HOME_CITY,             L"address_home_city" },
  { ADDRESS_HOME_STATE,            L"address_home_state" },
  { ADDRESS_HOME_ZIP,              L"address_home_zip" },
  { ADDRESS_HOME_COUNTRY,          L"address_home_country" },
  { ADDRESS_BILLING_LINE1,         L"address_billing_line1" },
  { ADDRESS_BILLING_LINE2,         L"address_billing_line2" },
  { ADDRESS_BILLING_CITY,          L"address_billing_city" },
  { ADDRESS_BILLING_STATE,         L"address_billing_state" },
  { ADDRESS_BILLING_ZIP,           L"address_billing_zip" },
  { ADDRESS_BILLING_COUNTRY,       L"address_billing_country" },
  { CREDIT_CARD_NAME,              L"credit_card_name" },
  { CREDIT_CARD_NUMBER,            L"credit_card_number" },
  { CREDIT_CARD_EXP_MONTH,         L"credit_card_exp_month" },
  { CREDIT_CARD_EXP_4_DIGIT_YEAR,  L"credit_card_exp_4_digit_year" },
  { CREDIT_CARD_TYPE,              L"credit_card_type" },
  // We do not import verification code.
};

typedef std::map<std::wstring, AutoFillFieldType> RegToFieldMap;

bool ImportSingleProfile(FormGroup* profile,
                         RegKey* key,
                         const RegToFieldMap& reg_to_field ) {
  DCHECK(profile != NULL);
  if (!key->Valid())
    return false;

  bool has_non_empty_fields = false;

  for (uint32 value_index = 0; value_index < key->ValueCount(); ++value_index) {
    std::wstring value_name;
    if (!key->ReadName(value_index, &value_name))
      continue;
    RegToFieldMap::const_iterator it = reg_to_field.find(value_name);
    if (it == reg_to_field.end())
      continue;  // This field is not imported.
    string16 field_value = ReadAndDecryptValue(key, value_name.c_str());
    if (!field_value.empty()) {
      has_non_empty_fields = true;
      profile->SetInfo(AutoFillType(it->second), field_value);
    }
  }
  return has_non_empty_fields;
}

// Imports profiles from the IE toolbar and stores them. Asynchronous
// if PersonalDataManager has not been loaded yet. Deletes itself on completion.
class AutoFillImporter : public PersonalDataManager::Observer {
 public:
  explicit AutoFillImporter(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {
      personal_data_manager_->SetObserver(this);
  }

  bool ImportProfiles() {
    if (!ImportCurrentUserProfiles(&profiles_, &credit_cards_)) {
      delete this;
      return false;
    }
    if (personal_data_manager_->IsDataLoaded())
      OnPersonalDataLoaded();
    return true;
  }

  // PersonalDataManager::Observer methods:
  virtual void OnPersonalDataLoaded() {
    if (!profiles_.empty())
      personal_data_manager_->SetProfiles(&profiles_);
    if (!credit_cards_.empty())
      personal_data_manager_->SetCreditCards(&credit_cards_);
    delete this;
  }

 private:
  ~AutoFillImporter() {
    personal_data_manager_->RemoveObserver(this);
  }

  PersonalDataManager* personal_data_manager_;
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> credit_cards_;
};

}  // namespace

// Imports AutoFill profiles and credit cards from IE Toolbar if present and not
// password protected. Returns true if data is successfully retrieved. False if
// there is no data, data is password protected or error occurred.
bool ImportCurrentUserProfiles(std::vector<AutoFillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards) {
  DCHECK(profiles);
  DCHECK(credit_cards);

  // Create a map of possible fields for a quick access.
  RegToFieldMap reg_to_field;
  for (size_t i = 0; i < arraysize(profile_reg_values); ++i) {
    reg_to_field[std::wstring(profile_reg_values[i].reg_value_name)] =
        profile_reg_values[i].field_type;
  }

  RegistryKeyIterator iterator_profiles(HKEY_CURRENT_USER, kProfileKey);
  for (; iterator_profiles.Valid(); ++iterator_profiles) {
    std::wstring key_name(kProfileKey);
    key_name.append(L"\\");
    key_name.append(iterator_profiles.Name());
    RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
    AutoFillProfile profile;
    if (ImportSingleProfile(&profile, &key, reg_to_field)) {
      // Combine phones into whole phone #.
      string16 phone;
      phone = profile.GetFieldText(AutoFillType(PHONE_HOME_COUNTRY_CODE));
      phone.append(profile.GetFieldText(AutoFillType(PHONE_HOME_CITY_CODE)));
      phone.append(profile.GetFieldText(AutoFillType(PHONE_HOME_NUMBER)));
      profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER), phone);
      phone = profile.GetFieldText(AutoFillType(PHONE_FAX_COUNTRY_CODE));
      phone.append(profile.GetFieldText(AutoFillType(PHONE_FAX_CITY_CODE)));
      phone.append(profile.GetFieldText(AutoFillType(PHONE_FAX_NUMBER)));
      profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER), phone);
      profiles->push_back(profile);
    }
  }
  string16 password_hash;
  string16 salt;
  RegKey cc_key(HKEY_CURRENT_USER, kCreditCardKey, KEY_READ);
  if (cc_key.Valid()) {
    password_hash = ReadAndDecryptValue(&cc_key, kPasswordHashValue);
    salt = ReadAndDecryptValue(&cc_key, kSaltValue);
  }

  // We import CC profiles only if they are not password protected.
  if (password_hash.empty() && IsEmptySalt(salt)) {
    RegistryKeyIterator iterator_cc(HKEY_CURRENT_USER, kCreditCardKey);
    for (; iterator_cc.Valid(); ++iterator_cc) {
      std::wstring key_name(kCreditCardKey);
      key_name.append(L"\\");
      key_name.append(iterator_cc.Name());
      RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
      CreditCard credit_card;
      if (ImportSingleProfile(&credit_card, &key, reg_to_field)) {
        string16 cc_number = credit_card.GetFieldText(
            AutoFillType(CREDIT_CARD_NUMBER));

        if (!cc_number.empty()) {
          // No additional password, and CC# is not empty, decrypt CC#.
          cc_number = DecryptCCNumber(cc_number);
        }
        credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER), cc_number);
        if (!cc_number.empty())
          credit_cards->push_back(credit_card);
      }
    }
  }
  return (profiles->size() + credit_cards->size()) > 0;
}

bool ImportAutofillDataWin(PersonalDataManager* pdm) {
  AutoFillImporter *importer = new AutoFillImporter(pdm);
  // importer will self delete.
  return importer->ImportProfiles();
}

