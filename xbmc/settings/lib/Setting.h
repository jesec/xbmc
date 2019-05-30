#pragma once
/*
 *      Copyright (C) 2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ISetting.h"
#include "ISettingCallback.h"
#include "ISettingControl.h"
#include "SettingDefinitions.h"
#include "SettingDependency.h"
#include "SettingUpdate.h"
#include "threads/SharedSection.h"

/*!
 \ingroup settings
 \brief Basic setting types available in the settings system.
 */
typedef enum {
  SettingTypeNone = 0,
  SettingTypeBool,
  SettingTypeInteger,
  SettingTypeNumber,
  SettingTypeString,
  SettingTypeAction,
  SettingTypeList,
  SettingTypeReference
} SettingType;

/*!
 \ingroup settings
 \brief Levels which every setting is assigned to.
 */
typedef enum {
  SettingLevelBasic  = 0,
  SettingLevelStandard,
  SettingLevelAdvanced,
  SettingLevelExpert,
  SettingLevelInternal
} SettingLevel;

typedef enum {
  SettingOptionsTypeNone = 0,
  SettingOptionsTypeStaticTranslatable,
  SettingOptionsTypeStatic,
  SettingOptionsTypeDynamic
} SettingOptionsType;

class CSetting;
typedef std::shared_ptr<CSetting> SettingPtr;
typedef std::shared_ptr<const CSetting> SettingConstPtr;
typedef std::vector<std::shared_ptr<CSetting>> SettingList;

/*!
 \ingroup settings
 \brief Setting base class containing all the properties which are common to
 all settings independent of the setting type.
 */
class CSetting : public ISetting,
                 protected ISettingCallback,
                 public std::enable_shared_from_this<CSetting>
{
public:
  CSetting(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSetting(const std::string &id, const CSetting &setting);
  virtual ~CSetting();

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const = 0;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual int GetType() const = 0;
  virtual bool FromString(const std::string &value) = 0;
  virtual std::string ToString() const = 0;
  virtual bool Equals(const std::string &value) const = 0;
  virtual bool CheckValidity(const std::string &value) const = 0;
  virtual void Reset() = 0;

  int GetLabel() const { return m_label; }
  void SetLabel(int label) { m_label = label; }
  int GetHelp() const { return m_help; }
  void SetHelp(int help) { m_help = help; }
  bool IsEnabled() const;
  void SetEnabled(bool enabled);
  bool IsDefault() const { return !m_changed; }
  const std::string& GetParent() const { return m_parentSetting; }
  void SetParent(const std::string& parentSetting) { m_parentSetting = parentSetting; }
  SettingLevel GetLevel() const { return m_level; }
  void SetLevel(SettingLevel level) { m_level = level; }
  std::shared_ptr<const ISettingControl> GetControl() const { return m_control; }
  std::shared_ptr<ISettingControl> GetControl() { return m_control; }
  void SetControl(std::shared_ptr<ISettingControl> control) { m_control = control; }
  const SettingDependencies& GetDependencies() const { return m_dependencies; }
  void SetDependencies(const SettingDependencies &dependencies) { m_dependencies = dependencies; }
  const std::set<CSettingUpdate>& GetUpdates() const { return m_updates; }

  void SetCallback(ISettingCallback *callback) { m_callback = callback; }

  // overrides of ISetting
  virtual bool IsVisible() const override;

  // implementation of ISettingCallback
  virtual void OnSettingAction(std::shared_ptr<const CSetting> setting) override;

protected:
  // implementation of ISettingCallback
  virtual bool OnSettingChanging(std::shared_ptr<const CSetting> setting) override;
  virtual void OnSettingChanged(std::shared_ptr<const CSetting> setting) override;
  virtual bool OnSettingUpdate(std::shared_ptr<CSetting> setting, const char *oldSettingId, const TiXmlNode *oldSettingNode) override;
  virtual void OnSettingPropertyChanged(std::shared_ptr<const CSetting> setting, const char *propertyName) override;

  void Copy(const CSetting &setting);

  template<class TSetting>
  std::shared_ptr<TSetting> shared_from_base()
  {
    return std::static_pointer_cast<TSetting>(shared_from_this());
  }

  ISettingCallback *m_callback;
  int m_label;
  int m_help;
  bool m_enabled;
  std::string m_parentSetting;
  SettingLevel m_level;
  std::shared_ptr<ISettingControl> m_control;
  SettingDependencies m_dependencies;
  std::set<CSettingUpdate> m_updates;
  bool m_changed;
  CSharedSection m_critical;
};

typedef std::shared_ptr<CSetting> SettingPtr;
typedef std::vector<SettingPtr> SettingList;

template<typename TValue, SettingType TSettingType>
class CTraitedSetting : public CSetting
{
public:
  typedef TValue Value;

  // implementation of CSetting
  virtual int GetType() const override { return TSettingType; }

  static SettingType Type() { return TSettingType; }

protected:
  CTraitedSetting(const std::string &id, CSettingsManager *settingsManager = NULL)
    : CSetting(id, settingsManager)
  { }
  CTraitedSetting(const std::string &id, const CTraitedSetting &setting)
    : CSetting(id, setting)
  { }
  virtual ~CTraitedSetting() { }
};

class CSettingReference : public CSetting
{
public:
  CSettingReference(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingReference(const std::string &id, const CSettingReference &setting);
  virtual ~CSettingReference() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual int GetType() const override { return SettingTypeReference; }
  virtual bool FromString(const std::string &value) override { return false; }
  virtual std::string ToString() const override { return ""; }
  virtual bool Equals(const std::string &value) const override { return false; }
  virtual bool CheckValidity(const std::string &value) const override { return false; }
  virtual void Reset() override { }

  const std::string& GetReferencedId() const { return m_referencedId; }
  void SetReferencedId(const std::string& referencedId) { m_referencedId = referencedId; }

private:
  std::string m_referencedId;
};

/*!
 \ingroup settings
 \brief List setting implementation
 \sa CSetting
 */
class CSettingList : public CSetting
{
public:
  CSettingList(const std::string &id, std::shared_ptr<CSetting> settingDefinition, CSettingsManager *settingsManager = NULL);
  CSettingList(const std::string &id, std::shared_ptr<CSetting> settingDefinition, int label, CSettingsManager *settingsManager = NULL);
  CSettingList(const std::string &id, const CSettingList &setting);
  virtual ~CSettingList();

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual int GetType() const override { return SettingTypeList; }
  virtual bool FromString(const std::string &value) override;
  virtual std::string ToString() const override;
  virtual bool Equals(const std::string &value) const override;
  virtual bool CheckValidity(const std::string &value) const override;
  virtual void Reset() override;
  
  int GetElementType() const;
  std::shared_ptr<CSetting> GetDefinition() { return m_definition; }
  std::shared_ptr<const CSetting> GetDefinition() const { return m_definition; }
  void SetDefinition(std::shared_ptr<CSetting> definition) { m_definition = definition; }

  const std::string& GetDelimiter() const { return m_delimiter; }
  void SetDelimiter(const std::string &delimiter) { m_delimiter = delimiter; }
  int GetMinimumItems() const { return m_minimumItems; }
  void SetMinimumItems(int minimumItems) { m_minimumItems = minimumItems; }
  int GetMaximumItems() const { return m_maximumItems; }
  void SetMaximumItems(int maximumItems) { m_maximumItems = maximumItems; }
  
  bool FromString(const std::vector<std::string> &value);

  const SettingList& GetValue() const { return m_values; }
  bool SetValue(const SettingList &values);
  const SettingList& GetDefault() const { return m_defaults; }
  void SetDefault(const SettingList &values);

protected:
  void copy(const CSettingList &setting);
  static void copy(const SettingList &srcValues, SettingList &dstValues);
  bool fromString(const std::string &strValue, SettingList &values) const;
  bool fromValues(const std::vector<std::string> &strValues, SettingList &values) const;
  std::string toString(const SettingList &values) const;

  SettingList m_values;
  SettingList m_defaults;
  std::shared_ptr<CSetting> m_definition;
  std::string m_delimiter;
  int m_minimumItems;
  int m_maximumItems;
};

/*!
 \ingroup settings
 \brief Boolean setting implementation.
 \sa CSetting
 */
class CSettingBool : public CTraitedSetting<bool, SettingTypeBool>
{
public:
  CSettingBool(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingBool(const std::string &id, const CSettingBool &setting);
  CSettingBool(const std::string &id, int label, bool value, CSettingsManager *settingsManager = NULL);
  virtual ~CSettingBool() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual bool FromString(const std::string &value) override;
  virtual std::string ToString() const override;
  virtual bool Equals(const std::string &value) const override;
  virtual bool CheckValidity(const std::string &value) const override;
  virtual void Reset() override { SetValue(m_default); }

  bool GetValue() const { CSharedLock lock(m_critical); return m_value; }
  bool SetValue(bool value);
  bool GetDefault() const { return m_default; }
  void SetDefault(bool value);

private:
  void copy(const CSettingBool &setting);
  bool fromString(const std::string &strValue, bool &value) const;

  bool m_value;
  bool m_default;
};

/*!
 \ingroup settings
 \brief Integer setting implementation
 \sa CSetting
 */
class CSettingInt : public CTraitedSetting<int, SettingTypeInteger>
{
public:
  CSettingInt(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingInt(const std::string &id, const CSettingInt &setting);
  CSettingInt(const std::string &id, int label, int value, CSettingsManager *settingsManager = NULL);
  CSettingInt(const std::string &id, int label, int value, int minimum, int step, int maximum, CSettingsManager *settingsManager = NULL);
  CSettingInt(const std::string &id, int label, int value, const TranslatableIntegerSettingOptions &options, CSettingsManager *settingsManager = NULL);
  virtual ~CSettingInt() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual bool FromString(const std::string &value) override;
  virtual std::string ToString() const override;
  virtual bool Equals(const std::string &value) const override;
  virtual bool CheckValidity(const std::string &value) const override;
  virtual bool CheckValidity(int value) const;
  virtual void Reset() override { SetValue(m_default); }

  int GetValue() const { CSharedLock lock(m_critical); return m_value; }
  bool SetValue(int value);
  int GetDefault() const { return m_default; }
  void SetDefault(int value);

  int GetMinimum() const { return m_min; }
  void SetMinimum(int minimum) { m_min = minimum; }
  int GetStep() const { return m_step; }
  void SetStep(int step) { m_step = step; }
  int GetMaximum() const { return m_max; }
  void SetMaximum(int maximum) { m_max = maximum; }

  SettingOptionsType GetOptionsType() const;
  const TranslatableIntegerSettingOptions& GetTranslatableOptions() const { return m_translatableOptions; }
  void SetTranslatableOptions(const TranslatableIntegerSettingOptions &options) { m_translatableOptions = options; }
  const IntegerSettingOptions& GetOptions() const { return m_options; }
  void SetOptions(const IntegerSettingOptions &options) { m_options = options; }
  const std::string& GetOptionsFillerName() const { return m_optionsFillerName; }
  void SetOptionsFillerName(const std::string &optionsFillerName, void *data = NULL)
  {
    m_optionsFillerName = optionsFillerName;
    m_optionsFillerData = data;
  }
  void SetOptionsFiller(IntegerSettingOptionsFiller optionsFiller, void *data = NULL)
  {
    m_optionsFiller = optionsFiller;
    m_optionsFillerData = data;
  }
  IntegerSettingOptions UpdateDynamicOptions();

private:
  void copy(const CSettingInt &setting);
  static bool fromString(const std::string &strValue, int &value);

  int m_value;
  int m_default;
  int m_min;
  int m_step;
  int m_max;
  TranslatableIntegerSettingOptions m_translatableOptions;
  IntegerSettingOptions m_options;
  std::string m_optionsFillerName;
  IntegerSettingOptionsFiller m_optionsFiller;
  void *m_optionsFillerData;
  IntegerSettingOptions m_dynamicOptions;
};

/*!
 \ingroup settings
 \brief Real number setting implementation.
 \sa CSetting
 */
class CSettingNumber : public CTraitedSetting<double, SettingTypeNumber>
{
public:
  CSettingNumber(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingNumber(const std::string &id, const CSettingNumber &setting);
  CSettingNumber(const std::string &id, int label, float value, CSettingsManager *settingsManager = NULL);
  CSettingNumber(const std::string &id, int label, float value, float minimum, float step, float maximum, CSettingsManager *settingsManager = NULL);
  virtual ~CSettingNumber() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual bool FromString(const std::string &value) override;
  virtual std::string ToString() const override;
  virtual bool Equals(const std::string &value) const override;
  virtual bool CheckValidity(const std::string &value) const override;
  virtual bool CheckValidity(double value) const;
  virtual void Reset() override { SetValue(m_default); }

  double GetValue() const { CSharedLock lock(m_critical); return m_value; }
  bool SetValue(double value);
  double GetDefault() const { return m_default; }
  void SetDefault(double value);

  double GetMinimum() const { return m_min; }
  void SetMinimum(double minimum) { m_min = minimum; }
  double GetStep() const { return m_step; }
  void SetStep(double step) { m_step = step; }
  double GetMaximum() const { return m_max; }
  void SetMaximum(double maximum) { m_max = maximum; }

private:
  virtual void copy(const CSettingNumber &setting);
  static bool fromString(const std::string &strValue, double &value);

  double m_value;
  double m_default;
  double m_min;
  double m_step;
  double m_max;
};

/*!
 \ingroup settings
 \brief String setting implementation.
 \sa CSetting
 */
class CSettingString : public CTraitedSetting<std::string, SettingTypeString>
{
public:
  CSettingString(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingString(const std::string &id, const CSettingString &setting);
  CSettingString(const std::string &id, int label, const std::string &value, CSettingsManager *settingsManager = NULL);
  virtual ~CSettingString() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual bool FromString(const std::string &value) override { return SetValue(value); }
  virtual std::string ToString() const override { return m_value; }
  virtual bool Equals(const std::string &value) const override { return m_value == value; }
  virtual bool CheckValidity(const std::string &value) const override;
  virtual void Reset() override { SetValue(m_default); }

  virtual const std::string& GetValue() const { CSharedLock lock(m_critical); return m_value; }
  virtual bool SetValue(const std::string &value);
  virtual const std::string& GetDefault() const { return m_default; }
  virtual void SetDefault(const std::string &value);

  virtual bool AllowEmpty() const { return m_allowEmpty; }
  void SetAllowEmpty(bool allowEmpty) { m_allowEmpty = allowEmpty; }

  SettingOptionsType GetOptionsType() const;
  const TranslatableStringSettingOptions& GetTranslatableOptions() const { return m_translatableOptions; }
  void SetTranslatableOptions(const TranslatableStringSettingOptions &options) { m_translatableOptions = options; }
  const StringSettingOptions& GetOptions() const { return m_options; }
  void SetOptions(const StringSettingOptions &options) { m_options = options; }
  const std::string& GetOptionsFillerName() const { return m_optionsFillerName; }
  void SetOptionsFillerName(const std::string &optionsFillerName, void *data = NULL)
  {
    m_optionsFillerName = optionsFillerName;
    m_optionsFillerData = data;
  }
  void SetOptionsFiller(StringSettingOptionsFiller optionsFiller, void *data = NULL)
  {
    m_optionsFiller = optionsFiller;
    m_optionsFillerData = data;
  }
  StringSettingOptions UpdateDynamicOptions();

protected:
  virtual void copy(const CSettingString &setting);

  std::string m_value;
  std::string m_default;
  bool m_allowEmpty;
  TranslatableStringSettingOptions m_translatableOptions;
  StringSettingOptions m_options;
  std::string m_optionsFillerName;
  StringSettingOptionsFiller m_optionsFiller;
  void *m_optionsFillerData;
  StringSettingOptions m_dynamicOptions;
};

/*!
 \ingroup settings
 \brief Action setting implementation.

 A setting action will trigger a call to the OnSettingAction() callback method
 when activated.

 \sa CSetting
 */
class CSettingAction : public CSetting
{
public:
  CSettingAction(const std::string &id, CSettingsManager *settingsManager = NULL);
  CSettingAction(const std::string &id, int label, CSettingsManager *settingsManager = NULL);
  CSettingAction(const std::string &id, const CSettingAction &setting);
  virtual ~CSettingAction() { }

  virtual std::shared_ptr<CSetting> Clone(const std::string &id) const override;

  virtual bool Deserialize(const TiXmlNode *node, bool update = false) override;

  virtual int GetType() const override { return SettingTypeAction; }
  virtual bool FromString(const std::string &value) override { return CheckValidity(value); }
  virtual std::string ToString() const override { return ""; }
  virtual bool Equals(const std::string &value) const override { return value.empty(); }
  virtual bool CheckValidity(const std::string &value) const override { return value.empty(); }
  virtual void Reset() override { }

  bool HasData() const { return !m_data.empty(); }
  const std::string& GetData() const { return m_data; }
  void SetData(const std::string& data) { m_data = data; }

protected:
  std::string m_data;
};
