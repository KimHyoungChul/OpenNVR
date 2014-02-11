#include <moment-ffmpeg/inc.h>

#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/time_checker.h>
#include <moment-ffmpeg/naming_scheme.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_channelcheck ("mod_ffmpeg.channelcheck", LogLevel::E);

bool ChannelChecker::writeIdx(ChannelFileSummary & files_existence,
                              StRef<String> const dir_name, StRef<String> const channel_name)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    StRef<String> const idx_file = st_makeString (dir_name, "/", channel_name, "/", channel_name, ".idx");
    std::string path = idx_file->cstr();

    std::ofstream idxFile;
    idxFile.open(path);
    if (idxFile.is_open())
    {
        ChannelFileSummary::iterator it = files_existence.begin();
        for(it; it != files_existence.end(); ++it)
        {
            idxFile << it->first;
            idxFile << "|";
            idxFile << it->second.first;
            idxFile << "|";
            idxFile << it->second.second;
            idxFile << std::endl;
        }
        idxFile.close();
        logD(channelcheck, _func_, "write successful, idxfile: ", path.c_str());
        bRes = true;
    }
    else
    {
        logD(channelcheck, _func_, "fail to open idxFile: ", path.c_str());
        bRes = false;
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.writeIdx exectime = [", t, "]");

    return bRes;
}

bool ChannelChecker::readIdx(StRef<String> const dir_name, StRef<String> const channel_name)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    StRef<String> const idx_file = st_makeString (dir_name, "/", channel_name, "/", channel_name, ".idx");
    std::string path = idx_file->cstr();
    std::ifstream idxFile;
    idxFile.open(path);

    std::string delimiter = "|";
    if (idxFile.is_open())
    {
        std::string line;
        while ( std::getline (idxFile, line) )
        {
            StRef<String> strToken;
            int initTime = 0;
            int endTime = 0;
            size_t pos = 0;

            pos = line.rfind(delimiter);
            strToken = st_makeString(line.substr(pos + delimiter.length()).c_str());
            strToInt32_safe(strToken->cstr(), &endTime);
            line.erase(pos);

            pos = line.rfind(delimiter);
            strToken = st_makeString(line.substr(pos + delimiter.length()).c_str());
            strToInt32_safe(strToken->cstr(), &initTime);
            line.erase(pos);

            m_chFileDiskTimes[line] = std::make_pair(std::string(dir_name->cstr()), std::make_pair(initTime, endTime));
        }
        idxFile.close();
        logD(channelcheck, _func_, "read successful, idxfile: ", path.c_str());
        bRes = true;
    }
    else
    {
        logD(channelcheck, _func_, "fail to open idxFile: ", path.c_str());
        bRes = false;
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_, "NvrData.writeIdx exectime = [", t, "]");

    return bRes;
}

ChannelChecker::ChannelFileTimes
ChannelChecker::getChannelExistence()
{
    logD(channelcheck, _func_,"getChannelExistence");

    TimeChecker tc;tc.Start();

    cleanCache();
    updateLastRecordInCache();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"getChannelExistence exectime = [", t, "]");

    return m_chTimes;
}

ChannelChecker::ChannelFileDiskTimes
ChannelChecker::getChannelFileDiskTimes ()
{
    logD(channelcheck, _func_,"getChannelFileDiskTimes");

    TimeChecker tc;tc.Start();

    cleanCache();
    updateLastRecordInCache();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"getChannelFileDiskTimes exectime = [", t, "]");

    return m_chFileDiskTimes;
}

ChannelChecker::ChannelFileSummary
ChannelChecker::getChFileSummary(const std::string & dirname)
{
    logD(channelcheck, _func_,"getChFileSummary");

    ChannelFileSummary chFileSummary;
    for(ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin(); itr != m_chFileDiskTimes.end(); itr++)
    {
        if(itr->second.first.compare(dirname) == 0)
        {
            chFileSummary[itr->first] = std::make_pair(itr->second.second.first, itr->second.second.second);
        }
    }

    return chFileSummary;
}

ChannelChecker::CheckResult
ChannelChecker::initCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    m_mutex.lock();

    TimeChecker tc;tc.Start();

    std::string curPath = m_recpathConfig->GetNextPath();
    while(curPath.length() != 0)
    {
        StRef<String> strRecDir = st_makeString(curPath.c_str());

        readIdx(strRecDir, m_channel_name);

        curPath = m_recpathConfig->GetNextPath(curPath.c_str());
    }

    if(!m_chFileDiskTimes.empty()) // if cache is not empty, then we add only new records
    {
        ChannelFileDiskTimes::reverse_iterator itr = m_chFileDiskTimes.rbegin();
        StRef<String> strLastfile = st_makeString(itr->first.c_str());
        StRef<String> strRecDir = st_makeString(itr->second.first.c_str());
        addRecordInCache(strLastfile, strRecDir, true);
    }

    recreateExistence();
    concatenateSuccessiveIntervals();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.initCache exectime = [", t, "]");
    m_mutex.unlock();

    return CheckResult_Success;
}

bool
ChannelChecker::DeleteFromCache(const std::string & dirName, const std::string & fileName)
{
    logD(channelcheck, _func_,"filename: [", dirName.c_str(), "/", fileName.c_str(), "]");

    m_mutex.lock();

    m_chFileDiskTimes.erase(fileName);
    ChannelFileSummary chFileSummary = getChFileSummary(dirName);
    StRef<String> strRecDir = st_makeString(dirName.c_str());
    writeIdx(chFileSummary, strRecDir, m_channel_name);

    recreateExistence();
    concatenateSuccessiveIntervals();

    m_mutex.unlock();

    return true;
}

bool
ChannelChecker::updateLastRecordInCache()
{
    logD(channelcheck, _func_);

    m_mutex.lock();

    ChannelFileDiskTimes::reverse_iterator itr = m_chFileDiskTimes.rbegin();

    if(itr != m_chFileDiskTimes.rend())
    {
        StRef<String> strRecDir = st_makeString(itr->second.first.c_str());
        StRef<String> path = st_makeString(itr->first.c_str());
        addRecordInCache(path, strRecDir, true);

        recreateExistence();
        concatenateSuccessiveIntervals();
    }

    m_mutex.unlock();

    return true;
}

ChannelChecker::CheckResult
ChannelChecker::completeCache(bool bUpdate)
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    m_mutex.lock();

    TimeChecker tc;tc.Start();

    CheckResult rez = CheckResult_Success;
    std::string curPath = m_recpathConfig->GetNextPath();
    while(curPath.length() != 0)
    {
        Time timeOfRecord = 0;
        ChannelFileSummary files_existence = getChFileSummary(curPath);
        if(!files_existence.empty())
        {
            std::string lastfile = files_existence.rbegin()->first;
            StRef<String> const flv_filename = st_makeString (lastfile.c_str(), ".flv");
            FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
            timeOfRecord = timeOfRecord / 1000000000LL;
        }

        NvrFileIterator file_iter;
        StRef<String> strRecDir = st_makeString(curPath.c_str());
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
        file_iter.init (vfs, m_channel_name->mem(), timeOfRecord);

        StRef<String> path = file_iter.getNext();
        while(path != NULL && !path->isNullString())
        {
            StRef<String> pathNext = file_iter.getNext();
            if (pathNext != NULL)
            {
                rez = addRecordInCache(path, strRecDir, true);
            }
            else
            {
                rez = addRecordInCache(path, strRecDir, bUpdate);
            }
            path = st_makeString(pathNext);
        }

        files_existence = getChFileSummary(curPath);
        writeIdx(files_existence, strRecDir, m_channel_name);

        curPath = m_recpathConfig->GetNextPath(curPath.c_str());
    }

    recreateExistence();
    concatenateSuccessiveIntervals();

    Time t;tc.Stop(&t);
    logD (channelcheck, _func_,"ChannelChecker.completeCache exectime = [", t, "]");

    m_mutex.unlock();

    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::cleanCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    m_mutex.lock();

    if(m_chFileDiskTimes.empty())
    {
        m_mutex.unlock();
        return CheckResult_Success;
    }

    TimeChecker tc;tc.Start();

    bool bNeedRecreateExistence = false;
    std::string curPath = m_recpathConfig->GetNextPath();
    while(curPath.length() != 0)
    {
        StRef<String> strRecDir = st_makeString(curPath.c_str());
        ConstMemory record_dir = strRecDir->mem();
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir);

        NvrFileIterator file_iter;
        file_iter.init (vfs, m_channel_name->mem(), 0); // get the oldest file

        StRef<String> path = file_iter.getNext();
        if(path != NULL)
        {
            Time timeOfFirstFile = 0;
            {
                StRef<String> const flv_filename = st_makeString (path, ".flv"); // replace by read from idx ??????
                FileNameToUnixTimeStamp().Convert(flv_filename, timeOfFirstFile);
                timeOfFirstFile = timeOfFirstFile / 1000000000LL;
            }

            ChannelFileDiskTimes::iterator it = m_chFileDiskTimes.begin();
            while(it != m_chFileDiskTimes.end())
            {
                if(it->second.first.compare(curPath) == 0)
                {
                    logD(channelcheck, _func_,"file [", it->first.c_str(), ",{", it->second.second.first, ",", it->second.second.second, "}]");
                    Time timeOfRecord = 0;
                    StRef<String> const flv_filename = st_makeString ((*it).first.c_str(), ".flv");
                    FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
                    timeOfRecord = timeOfRecord / 1000000000LL;

                    logD(channelcheck, _func_,"timeOfFirstFile: [", timeOfFirstFile, "], timeOfRecord: [", timeOfRecord, "]");
                    if(timeOfRecord < timeOfFirstFile)
                    {
                        logD(channelcheck, _func_,"clean it!");
                        m_chFileDiskTimes.erase(it++);

                        bNeedRecreateExistence = true;
                    }
                    else
                    {
                        logD(channelcheck, _func_,"all expired records have been deleted");
                        break;
                    }
                }
                else
                {
                    it++;
                }
            }
        }
        else
        {
            logD(channelcheck, _func_,"there is no files at all, clean all cache");

            ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin();
            while(itr != m_chFileDiskTimes.end())
            {
                if(itr->second.first.compare(curPath) == 0)
                {
                    m_chFileDiskTimes.erase(itr++);
                }
                else
                {
                    itr++;
                }
            }
            bNeedRecreateExistence = true;
        }

        ChannelFileSummary files_existence = getChFileSummary(curPath);
        writeIdx(files_existence, strRecDir, m_channel_name);

        curPath = m_recpathConfig->GetNextPath(curPath.c_str());
    }

    if(bNeedRecreateExistence)
    {
        recreateExistence();
        concatenateSuccessiveIntervals();
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.cleanCache exectime = [", t, "]");

    m_mutex.unlock();

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::recreateExistence()
{
    m_chTimes.clear();
    std::vector<std::pair<int,int>>().swap(m_chTimes);

    ChannelFileDiskTimes::iterator it;
    m_chTimes.reserve(m_chFileDiskTimes.size());
    for(it = m_chFileDiskTimes.begin(); it != m_chFileDiskTimes.end(); ++it)
    {
        m_chTimes.push_back(std::make_pair((*it).second.second.first,(*it).second.second.second));
    }
}

void
ChannelChecker::concatenateSuccessiveIntervals()
{
    if(m_chTimes.size() < 2)
        return;

    std::vector<std::pair<int,int>>::iterator it = m_chTimes.begin() + 1;
    while(1)
    {
        if (it == m_chTimes.end())
            break;

        if ((it->first - (it-1)->second) < 6)
        {
            std::pair<int,int> concatenated = std::make_pair<int,int>((int)(it-1)->first, (int)it->second);
            std::vector<std::pair<int,int>>::iterator prev = it-1;
            std::vector<std::pair<int,int>>::iterator prevPrev = it-2;
            m_chTimes.erase(it);
            m_chTimes.erase(prev);
            m_chTimes.insert(prevPrev+1, concatenated);
            it = m_chTimes.begin() + 1;
        }
        else ++it;
    }
}

ChannelChecker::CheckResult
ChannelChecker::addRecordInCache(StRef<String> path, StRef<String> strRecDir, bool bUpdate)
{
    logD(channelcheck, _func_,"addRecordInCache:", path->cstr());

    TimeChecker tc;tc.Start();

    if(m_chFileDiskTimes.find(std::string(path->cstr())) == m_chFileDiskTimes.end() || bUpdate)
    {
        StRef<String> const flv_filename = st_makeString (path, ".flv");
        StRef<String> flv_filenameFull = st_makeString(strRecDir, "/", flv_filename);

        FileReader fileReader;
        fileReader.Init(flv_filenameFull);
        Time timeOfRecord = 0;

        FileNameToUnixTimeStamp().Convert(flv_filenameFull, timeOfRecord);
        int const unixtime_timestamp_start = timeOfRecord / 1000000000LL;
        int const unixtime_timestamp_end = unixtime_timestamp_start + (int)fileReader.GetDuration();
        logD(channelcheck, _func_,"(int)fileReader.GetDuration() = [", (int)fileReader.GetDuration(), "]");

        m_chFileDiskTimes[std::string(path->cstr())] = std::make_pair(std::string(strRecDir->cstr()),
                                                                      std::make_pair(unixtime_timestamp_start, unixtime_timestamp_end));
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.addRecordInCache exectime = [", t, "]");

    return CheckResult_Success;
}

void
ChannelChecker::refreshTimerTick (void * const _self)
{
    ChannelChecker * const self = static_cast <ChannelChecker*> (_self);

    logD (channelcheck, _func_);

    self->cleanCache();
    self->completeCache(false);
}

mt_const void
ChannelChecker::init (Timers * const mt_nonnull timers, RecpathConfig * recpathConfig, StRef<String> & channel_name)
{
    m_recpathConfig = recpathConfig;
    m_channel_name = channel_name;

    logD(channelcheck, _func_,"m_channel_name=", m_channel_name);

    initCache();
    dumpData();

    m_timers = timers;

    m_timer_key = m_timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTick, this, this),
                      5    /* time_seconds */,
                      true /* periodical */,
                      false /* auto_delete */);
}

void ChannelChecker::dumpData()
{
    logD(channelcheck, _func_,"m_channel_name = [", m_channel_name, "]");

    m_mutex.lock();

    logD(channelcheck, _func_, "File times:");
    for(int i = 0; i < m_chTimes.size(); i++)
    {
        logD(channelcheck, _func_,"time interval = {", m_chTimes[i].first, ",", m_chTimes[i].second, "}");
    }

    logD(channelcheck, _func_, "File summary:");
    for(ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin(); itr != m_chFileDiskTimes.end(); ++itr)
    {
        logD(channelcheck, _func_,"file [", itr->first.c_str(), " on ", itr->second.first.c_str(), "] = {",
             itr->second.second.first, ",", itr->second.second.second, "}");
    }

    m_mutex.unlock();
}

ChannelChecker::ChannelChecker(): m_timers(this),m_timer_key(NULL), m_recpathConfig(NULL)
{

}

ChannelChecker::~ChannelChecker ()
{
    if (m_timer_key) {
        m_timers->deleteTimer (this->m_timer_key);
        m_timer_key = NULL;
    }
    m_recpathConfig = NULL;
}

}
