#ifndef FILEREADER_HH__
#define FILEREADER_HH__
#include <string>
#include <memory>
#include "eudaq/Platform.hh"
#include "eudaq/Configuration.hh"
#include "eudaq/Factory.hh"
#include "eudaq/Event.hh"


namespace eudaq{
  class FileReader;  
#ifndef EUDAQ_CORE_EXPORTS
  extern template class DLLEXPORT
  Factory<FileReader>;
  extern template DLLEXPORT std::map<uint32_t, typename Factory<FileReader>::UP(*)(std::string&)>&
  Factory<FileReader>::Instance<std::string&>();
  extern template DLLEXPORT std::map<uint32_t, typename Factory<FileReader>::UP(*)(std::string&&)>&
  Factory<FileReader>::Instance<std::string&&>();
#endif
  
  using FileReaderUP = Factory<FileReader>::UP;
  using FileReaderSP = Factory<FileReader>::SP;

  class DLLEXPORT FileReader{
  public:
    FileReader();
    virtual ~FileReader();
    void SetConfiguration(ConfigurationSPC c) {m_conf = c;};
    ConfigurationSPC  GetConfiguration() const {return m_conf;};
    virtual EventSPC GetNextEvent() = 0;
  private:
    ConfigurationSPC m_conf;
  };
}

#endif // FileReader_h__
