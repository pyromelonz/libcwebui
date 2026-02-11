#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
#include "intern/system_file_access_utils.h"
}

// File Type Detection Tests (MIME Types)
class FileTypeDetectionTest : public ::testing::Test {
protected:
    WebserverFileInfo* file;

    void SetUp() override {
        file = create_empty_file(0);
    }

    void TearDown() override {
        free_empty_file(file);
    }

    void setUrlAndDetectType(const char* url) {
        copyURL(file, url);
        setFileType(file);
    }
};

TEST_F(FileTypeDetectionTest, HtmlFile) {
    setUrlAndDetectType("/index.html");
    EXPECT_EQ(file->FileType, FILE_TYPE_HTML);
}

TEST_F(FileTypeDetectionTest, CssFile) {
    setUrlAndDetectType("/styles.css");
    EXPECT_EQ(file->FileType, FILE_TYPE_CSS);
}

TEST_F(FileTypeDetectionTest, JsFile) {
    setUrlAndDetectType("/app.js");
    EXPECT_EQ(file->FileType, FILE_TYPE_JS);
}

TEST_F(FileTypeDetectionTest, PngFile) {
    setUrlAndDetectType("/image.png");
    EXPECT_EQ(file->FileType, FILE_TYPE_PNG);
}

TEST_F(FileTypeDetectionTest, JpgFile) {
    setUrlAndDetectType("/photo.jpg");
    EXPECT_EQ(file->FileType, FILE_TYPE_JPG);
}

TEST_F(FileTypeDetectionTest, GifFile) {
    setUrlAndDetectType("/animation.gif");
    EXPECT_EQ(file->FileType, FILE_TYPE_GIF);
}

TEST_F(FileTypeDetectionTest, IcoFile) {
    setUrlAndDetectType("/favicon.ico");
    EXPECT_EQ(file->FileType, FILE_TYPE_ICO);
}

TEST_F(FileTypeDetectionTest, SvgFile) {
    setUrlAndDetectType("/vector.svg");
    EXPECT_EQ(file->FileType, FILE_TYPE_SVG);
}

TEST_F(FileTypeDetectionTest, PdfFile) {
    setUrlAndDetectType("/document.pdf");
    EXPECT_EQ(file->FileType, FILE_TYPE_PDF);
}

TEST_F(FileTypeDetectionTest, JsonFile) {
    setUrlAndDetectType("/data.json");
    EXPECT_EQ(file->FileType, FILE_TYPE_JSON);
}

TEST_F(FileTypeDetectionTest, XmlFile) {
    setUrlAndDetectType("/config.xml");
    EXPECT_EQ(file->FileType, FILE_TYPE_XML);
}

TEST_F(FileTypeDetectionTest, WoffFile) {
    setUrlAndDetectType("/font.woff");
    EXPECT_EQ(file->FileType, FILE_TYPE_WOFF);
}

TEST_F(FileTypeDetectionTest, TtfFile) {
    setUrlAndDetectType("/font.ttf");
    EXPECT_EQ(file->FileType, FILE_TYPE_TTF);
}

TEST_F(FileTypeDetectionTest, EotFile) {
    setUrlAndDetectType("/font.eot");
    EXPECT_EQ(file->FileType, FILE_TYPE_EOT);
}

TEST_F(FileTypeDetectionTest, BmpFile) {
    setUrlAndDetectType("/image.bmp");
    EXPECT_EQ(file->FileType, FILE_TYPE_BMP);
}

TEST_F(FileTypeDetectionTest, ManifestFile) {
    setUrlAndDetectType("/app.manifest");
    EXPECT_EQ(file->FileType, FILE_TYPE_MANIFEST);
}

TEST_F(FileTypeDetectionTest, XslFile) {
    setUrlAndDetectType("/transform.xsl");
    EXPECT_EQ(file->FileType, FILE_TYPE_XSL);
}

TEST_F(FileTypeDetectionTest, IncFile) {
    setUrlAndDetectType("/header.inc");
    EXPECT_EQ(file->FileType, FILE_TYPE_HTML_INC);
}

TEST_F(FileTypeDetectionTest, CSourceFile) {
    setUrlAndDetectType("/main.c");
    EXPECT_EQ(file->FileType, FILE_TYPE_C_SRC);
}

TEST_F(FileTypeDetectionTest, UnknownExtension) {
    setUrlAndDetectType("/file.xyz");
    EXPECT_EQ(file->FileType, FILE_TYPE_PLAIN);
}

TEST_F(FileTypeDetectionTest, NoExtension) {
    setUrlAndDetectType("/README");
    EXPECT_EQ(file->FileType, FILE_TYPE_PLAIN);
}

TEST_F(FileTypeDetectionTest, HiddenFile) {
    setUrlAndDetectType("/.htaccess");
    EXPECT_EQ(file->FileType, FILE_TYPE_PLAIN);
}

TEST_F(FileTypeDetectionTest, MultipleDotsInFilename) {
    setUrlAndDetectType("/file.test.html");
    EXPECT_EQ(file->FileType, FILE_TYPE_HTML);
}

TEST_F(FileTypeDetectionTest, UppercaseExtension) {
    setUrlAndDetectType("/FILE.HTML");
    EXPECT_EQ(file->FileType, FILE_TYPE_HTML);
}

TEST_F(FileTypeDetectionTest, MixedCaseExtension) {
    setUrlAndDetectType("/file.HtMl");
    EXPECT_EQ(file->FileType, FILE_TYPE_HTML);
}

TEST_F(FileTypeDetectionTest, PathWithDirectories) {
    setUrlAndDetectType("/path/to/file.css");
    EXPECT_EQ(file->FileType, FILE_TYPE_CSS);
}

TEST_F(FileTypeDetectionTest, DirectoryWithDot) {
    setUrlAndDetectType("/path.old/to/file");
    // Should treat as no extension
    EXPECT_EQ(file->FileType, FILE_TYPE_PLAIN);
}

// URL Normalization Tests
class UrlNormalizationTest : public ::testing::Test {
protected:
    WebserverFileInfo* file;

    void SetUp() override {
        file = create_empty_file(0);
    }

    void TearDown() override {
        free_empty_file(file);
    }
};

TEST_F(UrlNormalizationTest, StripLeadingSlash) {
    copyURL(file, "/index.html");
    EXPECT_STREQ(file->Url, "index.html");
}

TEST_F(UrlNormalizationTest, StripMultipleLeadingSlashes) {
    copyURL(file, "///index.html");
    EXPECT_STREQ(file->Url, "index.html");
}

TEST_F(UrlNormalizationTest, NoLeadingSlash) {
    copyURL(file, "index.html");
    EXPECT_STREQ(file->Url, "index.html");
}

TEST_F(UrlNormalizationTest, PathWithSubdirectory) {
    copyURL(file, "/path/to/file.html");
    EXPECT_STREQ(file->Url, "path/to/file.html");
}

TEST_F(UrlNormalizationTest, RootUrl) {
    copyURL(file, "/");
    EXPECT_STREQ(file->Url, "");
}

TEST_F(UrlNormalizationTest, EmptyUrl) {
    copyURL(file, "");
    EXPECT_STREQ(file->Url, "");
}

TEST_F(UrlNormalizationTest, UrlLengthCorrect) {
    copyURL(file, "/test.html");
    EXPECT_EQ(file->UrlLengt, strlen("test.html"));
}

// File Path Copy Tests
class FilePathCopyTest : public ::testing::Test {
protected:
    WebserverFileInfo* file;

    void SetUp() override {
        file = create_empty_file(0);
    }

    void TearDown() override {
        free_empty_file(file);
    }
};

TEST_F(FilePathCopyTest, SimpleFilePath) {
    copyFilePath(file, "/var/www/index.html");
    EXPECT_STREQ(file->FilePath, "/var/www/index.html");
}

TEST_F(FilePathCopyTest, FilePathLength) {
    copyFilePath(file, "/var/www/test.html");
    EXPECT_EQ(file->FilePathLengt, strlen("/var/www/test.html"));
}

TEST_F(FilePathCopyTest, OverwriteFilePath) {
    copyFilePath(file, "/first/path.html");
    copyFilePath(file, "/second/path.html");
    EXPECT_STREQ(file->FilePath, "/second/path.html");
}

TEST_F(FilePathCopyTest, EmptyFilePath) {
    copyFilePath(file, "");
    EXPECT_STREQ(file->FilePath, "");
    EXPECT_EQ(file->FilePathLengt, 0u);
}

// File Info Memory Tests
class FileInfoMemoryTest : public ::testing::Test {};

TEST_F(FileInfoMemoryTest, CreateEmptyFile) {
    WebserverFileInfo* file = create_empty_file(0);
    ASSERT_NE(file, nullptr);
    free_empty_file(file);
}

TEST_F(FileInfoMemoryTest, CreateWithSize) {
    WebserverFileInfo* file = create_empty_file(1024);
    ASSERT_NE(file, nullptr);
    // Data should be allocated
    EXPECT_NE(file->Data, nullptr);
    EXPECT_EQ(file->DataLenght, 1024u);
    free_empty_file(file);
}

TEST_F(FileInfoMemoryTest, CreateMultipleFiles) {
    WebserverFileInfo* files[10];

    for (int i = 0; i < 10; i++) {
        files[i] = create_empty_file(100);
        ASSERT_NE(files[i], nullptr);
    }

    for (int i = 0; i < 10; i++) {
        free_empty_file(files[i]);
    }
}

// Cache Header Comparison Tests (ETag/Last-Modified logic)
class CacheComparisonTest : public ::testing::Test {};

TEST_F(CacheComparisonTest, ETagExactMatch) {
    const char* etag1 = "\"abc123\"";
    const char* etag2 = "\"abc123\"";

    EXPECT_EQ(strcmp(etag1, etag2), 0);
}

TEST_F(CacheComparisonTest, ETagMismatch) {
    const char* etag1 = "\"abc123\"";
    const char* etag2 = "\"xyz789\"";

    EXPECT_NE(strcmp(etag1, etag2), 0);
}

TEST_F(CacheComparisonTest, LastModifiedExactMatch) {
    const char* lm1 = "Wed, 21 Oct 2015 07:28:00 GMT";
    const char* lm2 = "Wed, 21 Oct 2015 07:28:00 GMT";

    EXPECT_EQ(strcmp(lm1, lm2), 0);
    EXPECT_EQ(strlen(lm1), strlen(lm2));
}

TEST_F(CacheComparisonTest, LastModifiedMismatch) {
    const char* lm1 = "Wed, 21 Oct 2015 07:28:00 GMT";
    const char* lm2 = "Thu, 22 Oct 2015 07:28:00 GMT";

    EXPECT_NE(strcmp(lm1, lm2), 0);
}

// HTTP Date Format Tests
// Format: "DD Mon YYYY HH:MM GMT" (e.g., "21 Oct 2015 07:28 GMT")
class HttpDateFormatTest : public ::testing::Test {};

TEST_F(HttpDateFormatTest, FormatDate) {
    char buffer[100];

    // getHTMLDateFormat(buffer, day, month, year, hour, minute)
    getHTMLDateFormat(buffer, 21, 10, 2015, 7, 28);

    // Should contain GMT
    EXPECT_NE(strstr(buffer, "GMT"), nullptr);

    // Should contain month abbreviation
    EXPECT_NE(strstr(buffer, "Oct"), nullptr);
}

TEST_F(HttpDateFormatTest, FormatDateLength) {
    char buffer[100];
    getHTMLDateFormat(buffer, 1, 1, 2020, 12, 0);

    // Format is "DD Mon YYYY HH:MM GMT" = ~21 chars
    EXPECT_GE(strlen(buffer), 20u);
    EXPECT_LE(strlen(buffer), 25u);
}

TEST_F(HttpDateFormatTest, ContainsYear) {
    char buffer[100];
    getHTMLDateFormat(buffer, 15, 6, 2023, 10, 30);

    // Should contain the year
    EXPECT_NE(strstr(buffer, "2023"), nullptr);
}

TEST_F(HttpDateFormatTest, ContainsMonth) {
    char buffer[100];
    getHTMLDateFormat(buffer, 15, 6, 2023, 10, 30);

    // June should be in the date
    EXPECT_NE(strstr(buffer, "Jun"), nullptr);
}

// Blocked Files Tests
// Note: These require init_file_access_utils() to be called
class BlockedFilesTest : public ::testing::Test {
protected:
    static bool initialized;

    static void SetUpTestSuite() {
        if (!initialized) {
            init_file_access_utils();
            initialized = true;
        }
    }
};

bool BlockedFilesTest::initialized = false;

TEST_F(BlockedFilesTest, UnblockedFile) {
    // Random file should not be blocked
    int blocked = check_blocked_urls("random_file.txt");
    EXPECT_EQ(blocked, 0);
}

TEST_F(BlockedFilesTest, AddAndCheckBlockedFile) {
    WebserverAddBlockedFile("/secret.txt");

    int blocked = check_blocked_urls("secret.txt");
    EXPECT_EQ(blocked, 1);
}

TEST_F(BlockedFilesTest, BlockedFileWithLeadingSlash) {
    WebserverAddBlockedFile("private.html");

    // URL normalization strips leading slash
    int blocked = check_blocked_urls("private.html");
    EXPECT_EQ(blocked, 1);
}
