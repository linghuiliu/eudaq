#include "NiController.hh"
#include "eudaq/Producer.hh"

#include <cstdio>
#include <cstdlib>
#include <memory>

class NiProducer : public eudaq::Producer {
public:
  NiProducer(const std::string name, const std::string &runcontrol);
  ~NiProducer() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoReset() override;
  void DoTerminate() override;

  void DataLoop();

  static const uint32_t m_id_factory = eudaq::cstr2hash("NiProducer");
private:
  bool m_running;
  bool m_configured;
  std::thread m_thd_data;
  std::shared_ptr<NiController> ni_control;
  
  uint32_t datalength1;
  uint32_t datalength2;
  uint32_t ConfDataLength;
  std::vector<uint8_t> ConfDataError;

  uint32_t TriggerType;
  uint32_t Det;
  uint32_t Mode;
  uint32_t NiVersion;
  uint32_t NumBoards;
  uint32_t FPGADownload;
  uint32_t MimosaID[6];
  uint32_t MimosaEn[6];
  bool OneFrame;
  bool NiConfig;
  
  unsigned char conf_parameters[10];
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<NiProducer, const std::string&, const std::string&>(NiProducer::m_id_factory);
}

NiProducer::NiProducer(const std::string name, const std::string &runcontrol)
  : eudaq::Producer(name, runcontrol), m_running(false), m_configured(false){
  m_running = false;
  m_configured = false;
}

NiProducer::~NiProducer(){
  m_running = false;
  if(m_thd_data.joinable()){
    m_thd_data.join();
  }
}

void NiProducer::DataLoop(){
  ni_control->Start();
  bool isbegin = true;
  while(1){
    auto evup = eudaq::RawDataEvent::MakeUnique("NiRawDataEvent");
    datalength1 = ni_control->DataTransportClientSocket_ReadLength("priv");
    std::vector<uint8_t> mimosa_data_0(datalength1);
    mimosa_data_0 = ni_control->DataTransportClientSocket_ReadData(datalength1);
    datalength2 = ni_control->DataTransportClientSocket_ReadLength("priv");
    std::vector<uint8_t> mimosa_data_1(datalength2);
    mimosa_data_1 = ni_control->DataTransportClientSocket_ReadData(datalength2);
    auto ev = dynamic_cast<eudaq::RawDataEvent*>(evup.get());
    ev->AddBlock(0, mimosa_data_0);
    ev->AddBlock(1, mimosa_data_1);
    if(isbegin){
      isbegin = false;
      evup->SetBORE();
      evup->SetTag("DET", "MIMOSA26");
      evup->SetTag("MODE", "ZS2");
      evup->SetTag("BOARDS", NumBoards);
      for (size_t i = 0; i < 6; i++)
	evup->SetTag("ID" + std::to_string(i), std::to_string(MimosaID[i]));
      for (size_t i = 0; i < 6; i++)
	evup->SetTag("MIMOSA_EN" + std::to_string(i), std::to_string(MimosaEn[i]));
    }
    if(!m_running){
      evup->SetEORE();
      SendEvent(std::move(evup));
      break;
    }    
    SendEvent(std::move(evup));
  }
  ni_control->Stop();
}

void NiProducer::DoConfigure() {
  auto conf = GetConfiguration();  
  if (!m_configured) {
    ni_control = std::make_shared<NiController>();
    ni_control->GetProduserHostInfo();
    std::string addr = conf->Get("NiIPaddr", "localhost");
    uint16_t port = conf->Get("NiConfigSocketPort", 49248);
    ni_control->ConfigClientSocket_Open(addr, port);
    addr = conf->Get("NiIPaddr", "localhost");
    port = conf->Get("NiDataTransportSocketPort", 49250);
    ni_control->DatatransportClientSocket_Open(addr, port);
    std::cout << " " << std::endl;
    m_configured = true;
  }

  TriggerType = conf->Get("TriggerType", 255);
  Det = conf->Get("Det", 255);
  Mode = conf->Get("Mode", 255);
  NiVersion = conf->Get("NiVersion", 255);
  NumBoards = conf->Get("NumBoards", 255);
  FPGADownload = conf->Get("FPGADownload", 1);
  for (size_t i = 0; i < 6; i++) {
    MimosaID[i] = conf->Get("MimosaID_" + std::to_string(i + 1), 255);
    MimosaEn[i] = conf->Get("MimosaEn_" + std::to_string(i + 1), 255);
  }
  OneFrame = conf->Get("OneFrame", 255);

  std::cout << "Configuring ...(" << conf->Name() << ")" << std::endl;

  conf_parameters[0] = NiVersion;
  conf_parameters[1] = TriggerType;
  conf_parameters[2] = Det;
  conf_parameters[3] = MimosaEn[1];
  conf_parameters[4] = MimosaEn[2];
  conf_parameters[5] = MimosaEn[3];
  conf_parameters[6] = MimosaEn[4];
  conf_parameters[7] = MimosaEn[5];
  conf_parameters[8] = NumBoards;
  conf_parameters[9] = FPGADownload;

  unsigned char configur[5] = "conf";
  ni_control->ConfigClientSocket_Send(configur, sizeof(configur));
  ni_control->ConfigClientSocket_Send(conf_parameters,
				      sizeof(conf_parameters));

  ConfDataLength = ni_control->ConfigClientSocket_ReadLength("priv");
  ConfDataError = ni_control->ConfigClientSocket_ReadData(ConfDataLength);

  NiConfig = false;

  if ((ConfDataError[3] & 0x1) >> 0) {
    EUDAQ_ERROR("NI crate can not be configure: ErrorReceive Config");
    NiConfig = true;
  } // ErrorReceive Config
  if ((ConfDataError[3] & 0x2) >> 1) {
    EUDAQ_ERROR("NI crate can not be configure: Error FPGA open");
    NiConfig = true;
  } // Error FPGA open
  if ((ConfDataError[3] & 0x4) >> 2) {
    EUDAQ_ERROR("NI crate can not be configure: Error FPGA reset");
    NiConfig = true;
  } // Error FPGA reset
  if ((ConfDataError[3] & 0x8) >> 3) {
    EUDAQ_ERROR("NI crate can not be configure: Error FPGA download");
    NiConfig = true;
  } // Error FPGA download
  if ((ConfDataError[3] & 0x10) >> 4) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_0 Start");
    NiConfig = true;
  } // FIFO_0 Configure
  if ((ConfDataError[3] & 0x20) >> 5) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_1 Start");
    NiConfig = true;
  } // FIFO_0 Start
  if ((ConfDataError[3] & 0x40) >> 6) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_2 Start");
    NiConfig = true;
  } // FIFO_1 Configure
  if ((ConfDataError[3] & 0x80) >> 7) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_3 Start");
    NiConfig = true;
  } // FIFO_1 Start
  if ((ConfDataError[2] & 0x1) >> 0) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_4 Start");
    NiConfig = true;
  } // FIFO_2 Configure
  if ((ConfDataError[2] & 0x2) >> 1) {
    EUDAQ_ERROR("NI crate can not be configure: FIFO_5 Start");
    NiConfig = true;
  } // FIFO_2 Start

  if (NiConfig) {
    EUDAQ_THROW("NiProducer was Configured with ERRORs "+ conf->Name());
  }
}

void NiProducer::DoStartRun(){
  if(m_thd_data.joinable())
    EUDAQ_THROW("NiProducer::OnStartRun(): Last run is not stopped.");

  m_running = true;
  m_thd_data = std::thread(&NiProducer::DataLoop, this);
}

void NiProducer::DoStopRun() {
  m_running = false;
  if(m_thd_data.joinable())
    m_thd_data.join();
}


void NiProducer::DoReset(){
  m_running = false;
  if(m_thd_data.joinable())
    m_thd_data.join();
  m_configured = false;
  ni_control.reset();
}

void NiProducer::DoTerminate() {
  std::cout << "Terminate (press enter)" << std::endl;
  m_running = false;
  if(m_thd_data.joinable()){
    m_thd_data.join();
  }
  ni_control->DatatransportClientSocket_Close();
  ni_control->ConfigClientSocket_Close();
  eudaq::mSleep(1000);
}
