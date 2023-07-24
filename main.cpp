#include <iostream>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>
#include <archive.h>

class UpdaterException : public std::exception {
public:
    UpdaterException(std::string error): error(error) {}
    std::string getError() const {return error;}

private:
    std::string error;
};

void runUpdater();
std::string getLatestVersion();
void installUpdate();
void downloadUpdate();
void extractUpdate();
std::string getVersionFromFile(std::string path);
void downloadFile(std::string url, std::string path);
size_t writeData(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t displayDownloadProgress(void* ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded);
void extract(std::string filename);
int copyData(archive* src, archive* dst);

int main(int argc, char* argv[])
{
    try {
        runUpdater();
    }
    catch(const UpdaterException &exception) {
        std::cout << "ERROR: " << exception.getError() << std::string(30, ' ') << std::endl;
    }
    catch(const std::exception &exception) {
        std::cout << "ERROR: unknown error" << std::string(30, ' ') << std::endl;
    }

    #ifdef __WIN32
        system("pause");
    #endif

    return 0;
}

void runUpdater()
{
    std::cout << "Checking for updates..." << std::endl;
    std::filesystem::create_directory("update");

    std::string currentVersion = getVersionFromFile("version");
    std::string latestVersion = getLatestVersion();

    if(latestVersion != currentVersion) {
        std::cout << "Newer version " << latestVersion << " is available, installing..." << std::endl;
        installUpdate();
        std::cout << "\rUpdate complete                                 " << std::endl;
    }
    else {
        std::cout << "You have the latest version of Tails Adventure Remake" << std::endl;
    }
}

std::string getLatestVersion()
{
    const std::string versionURL = "https://raw.githubusercontent.com/TA-Remake/release/main/version";
    downloadFile(versionURL, "update/version");
    return getVersionFromFile("update/version");
}

void installUpdate()
{
    downloadUpdate();
    extract("update/update.zip");
}

void downloadUpdate()
{
    const std::string windowsReleaseURL = "https://raw.githubusercontent.com/TA-Remake/release/main/windows.zip";
    const std::string linuxURL = "https://raw.githubusercontent.com/TA-Remake/release/main/linux.zip";

    #ifdef __WIN32
        downloadFile(windowsReleaseURL, "update/update.zip");
    #else
        downloadFile(linuxReleaseURL, "update/update.zip");
    #endif
}

std::string getVersionFromFile(std::string path)
{
    std::ifstream fin(path);
    if(!fin.is_open()) {
        throw UpdaterException("failed to open file " + path);
    }

    std::string version = "";
    fin >> version;
    fin.close();
    return version;
}

void downloadFile(std::string url, std::string path)
{
    FILE* file = fopen(path.c_str(), "wb");
    if(!file) {
        throw UpdaterException("failed to open file " + path);
    }

    CURL* curl = curl_easy_init();
    if(!curl) {
        throw UpdaterException("failed to init curl");
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

    if(path == "update/update.zip") {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, false);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, displayDownloadProgress);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(file);

    if(res != CURLE_OK) {
        throw UpdaterException("failed to download file (error code " + std::to_string(res) + ")");
    }
}

size_t writeData(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

size_t displayDownloadProgress(void* ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded)
{
    static int prevPercent = -1;
    int percent = 0;
    if(totalToDownload != 0) {
        percent = 100 * (long long)(nowDownloaded) / (long long)(totalToDownload);
    }
    if(percent != prevPercent) {
        std::cout << "\rDownloading... " << percent << "% complete";
        prevPercent = percent;
    }
    return 0;
}

void extract(std::string filename)
{
    std::cout << "\rExtracting...                                    ";
    archive* src;
    archive* dst;
    archive_entry* entry;
    int flags;
    int result;

    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;

    src = archive_read_new();
    archive_read_support_format_all(src);
    archive_read_support_filter_all(src);
    dst = archive_write_disk_new();
    archive_write_disk_set_options(dst, flags);
    archive_write_disk_set_standard_lookup(dst);

    if(archive_read_open_filename(src, filename.c_str(), 10240)) {
        throw UpdaterException("failed to open archive");
    }
    
    while(true) {
        result = archive_read_next_header(src, &entry);
        if(result == ARCHIVE_EOF) {
            break;
        }
        if(result < ARCHIVE_WARN) {
            throw UpdaterException("unpack error: " + std::string(archive_error_string(src)));
        }

        result = archive_write_header(dst, entry);
        if(result < ARCHIVE_WARN) {
            throw UpdaterException("unpack error: " + std::string(archive_error_string(src)));
        }

        result = copyData(src, dst);
        if(result < ARCHIVE_WARN) {
            throw UpdaterException("unpack error: " + std::string(archive_error_string(src)));
        }

        result = archive_write_finish_entry(dst);
        if(result < ARCHIVE_WARN) {
            throw UpdaterException("unpack error: " + std::string(archive_error_string(src)));
        }
    }

    archive_read_close(src);
    archive_read_free(src);
    archive_write_close(dst);
    archive_write_free(dst);
}

int copyData(archive* src, archive* dst)
{
    int result;
    const void *buff;
    size_t size;
    la_int64_t offset;

    while(true) {
        result = archive_read_data_block(src, &buff, &size, &offset);
        if(result == ARCHIVE_EOF) {
            return ARCHIVE_OK;
        }
        if(result < ARCHIVE_WARN) {
            return result;
        }

        result = archive_write_data_block(dst, buff, size, offset);
        if(result < ARCHIVE_WARN) {
            return result;
        }
    }
}
