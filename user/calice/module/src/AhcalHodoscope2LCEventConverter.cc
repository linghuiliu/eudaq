#include "eudaq/LCEventConverter.hh"
#include "eudaq/RawEvent.hh"
#include "eudaq/Logger.hh"

namespace eudaq {

using namespace std;
using namespace lcio;

class AhcalHodoscope2LCEventConverter: public LCEventConverter {
   public:
      bool Converting(EventSPC d1, LCEventSP d2, ConfigurationSPC conf) const override;
      static const uint32_t m_id_factory = cstr2hash("HodoscopeRaw");
      private:
      LCCollectionVec* createCollectionVec(lcio::LCEvent &result, string colName, string dataDesc, time_t timestamp) const;
      void getScCALTemperatureSubEvent(const std::vector<uint8_t> &bl, LCCollectionVec *col) const;
      void getDataLCIOGenericObject(const std::vector<uint8_t> &bl, LCCollectionVec *col, int nblock) const;
      void getDataLCIOGenericObject(eudaq::RawDataEvent const *rawev, LCCollectionVec *col, int nblock, int nblock_max = 0) const;

};

namespace {
auto dummy0 = Factory<LCEventConverter>::
      Register<AhcalHodoscope2LCEventConverter>(AhcalHodoscope2LCEventConverter::m_id_factory);
}

static const char* EVENT_TYPE = "HodoscopeRaw";

class CaliceLCGenericObject: public lcio::LCGenericObjectImpl {
   public:
      CaliceLCGenericObject() {
         _typeName = EVENT_TYPE;
      }

      void setTags(std::string &s) {
         _dataDescription = s;
      }
      void setIntDataInt(std::vector<int> &vec) {
         _intVec.resize(vec.size());
         std::copy(vec.begin(), vec.end(), _intVec.begin());
      }

      std::string getTags() const {
         return _dataDescription;
      }
      const std::vector<int>& getDataInt() const {
         return _intVec;
      }
};

bool AhcalHodoscope2LCEventConverter::Converting(EventSPC d1, LCEventSP d2, ConfigurationSPC conf) const {
   // try to cast the Event
   auto& source = *(d1.get());
   auto& result = *(d2.get());

   eudaq::RawDataEvent const * rawev = 0;
   try {
      eudaq::RawDataEvent const & rawdataevent = dynamic_cast<eudaq::RawDataEvent const &>(source);
      rawev = &rawdataevent;
   } catch (std::bad_cast& e) {
      std::cout << e.what() << std::endl;
      return false;
   }
   // should check the type
   if (rawev->GetExtendWord() != eudaq::cstr2hash(EVENT_TYPE)) {
      cout << "AHCALHodoscopeConverter: type failed!" << endl;
      return false;
   }
//   return true;
   // no contents -ignore
   if (rawev->NumBlocks() != 2) return true;

   unsigned int nblock = 0;
   auto blockTimestamp = rawev->GetBlock(nblock++);
   auto blockData=rawev->GetBlock(nblock++);
   time_t timestamp = *(unsigned int *) (&blockTimestamp[0]);
   // check if the data is okay (checked at the producer level)
   int DAQquality = rawev->GetTag("DAQquality", 1);
   string colName = rawev->GetTag("SRC", string("UnidentifiedHodoscope"));
   string dataDesc = "DataDescription";

  // std::cout << "Colname=" << colName << std::endl;
   LCCollectionVec *col = 0;
   col = createCollectionVec(result, colName, dataDesc, timestamp);
   getDataLCIOGenericObject(blockData, col, nblock);
   return true;
}

LCCollectionVec* AhcalHodoscope2LCEventConverter::createCollectionVec(lcio::LCEvent &result, string colName, string dataDesc, time_t timestamp) const {
   LCCollectionVec *col = 0;
   try {
      // looking for existing collection
      col = dynamic_cast<IMPL::LCCollectionVec *>(result.getCollection(colName));
   } catch (DataNotAvailableException &e) {
      // create new collection
      col = new IMPL::LCCollectionVec(LCIO::LCGENERICOBJECT);
      result.addCollection(col, colName);
   }
   col->parameters().setValue("DataDescription", dataDesc);
   //add timestamp (set by the Producer, is EUDAQ, not real timestamp!!)
   struct tm *tms = localtime(&timestamp);
   char tmc[256];
   strftime(tmc, 256, "%a, %d %b %Y %T %z", tms);
   col->parameters().setValue("Timestamp", tmc);
   return col;
}

void AhcalHodoscope2LCEventConverter::getDataLCIOGenericObject(const std::vector<uint8_t> & bl, LCCollectionVec *col, int nblock) const {

   // further blocks should be data (currently limited to integer)

   vector<int> v;
   v.resize(bl.size() / sizeof(int));
   memcpy(&v[0], &bl[0], bl.size());

   CaliceLCGenericObject *obj = new CaliceLCGenericObject;
   obj->setIntDataInt(v);
   try {
      col->addElement(obj);
   } catch (ReadOnlyException &e) {
      cout << "CaliceGenericConverterPlugin: the collection to add is read only! skipped..." << endl;
      delete obj;
   }

}

void AhcalHodoscope2LCEventConverter::getDataLCIOGenericObject(eudaq::RawDataEvent const * rawev, LCCollectionVec *col, int nblock, int nblock_max) const {

   if (nblock_max == 0) nblock_max = rawev->NumBlocks();
   while (nblock < nblock_max) {
      // further blocks should be data (currently limited to integer)

      vector<int> v;
      auto bl = rawev->GetBlock(nblock++);
      v.resize(bl.size() / sizeof(int));
      memcpy(&v[0], &bl[0], bl.size());

      CaliceLCGenericObject *obj = new CaliceLCGenericObject;
      obj->setIntDataInt(v);
      try {
         col->addElement(obj);
      } catch (ReadOnlyException &e) {
         cout << "CaliceGenericConverterPlugin: the collection to add is read only! skipped..." << endl;
         delete obj;
      }
   }
}

}
