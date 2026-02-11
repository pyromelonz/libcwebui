#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// Tests for getNextTag function
class GetNextTagTest : public ::testing::Test {
protected:
    TAG_IDS tag;

    void SetUp() override {
        tag = (TAG_IDS)-1;
    }
};

// IF tag group tests
TEST_F(GetNextTagTest, FindIfTag) {
    const char* data = "before {if:condition} after";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_GT(pos, 0);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(GetNextTagTest, FindElseTag) {
    const char* data = "before {else} after";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_GT(pos, 0);
    EXPECT_EQ(tag, ELSE_TAG);
}

TEST_F(GetNextTagTest, FindEndifTag) {
    const char* data = "before {endif} after";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_GT(pos, 0);
    EXPECT_EQ(tag, ENDIF_TAG);
}

TEST_F(GetNextTagTest, NoIfTagFound) {
    const char* data = "no tags here";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_EQ(pos, -1);
}

TEST_F(GetNextTagTest, IfTagAtStart) {
    const char* data = "{if:x}content";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_EQ(pos, 4);  // Position after "{if:"
    EXPECT_EQ(tag, IF_TAG);
}

// LOOP tag group tests
TEST_F(GetNextTagTest, FindLoopTag) {
    const char* data = "before {loop:items} after";
    int pos = getNextTag(data, strlen(data), LOOP_TAG_GROUP, &tag);

    EXPECT_GT(pos, 0);
    EXPECT_EQ(tag, LOOP_TAG);
}

TEST_F(GetNextTagTest, FindEndloopTag) {
    const char* data = "before {endloop} after";
    int pos = getNextTag(data, strlen(data), LOOP_TAG_GROUP, &tag);

    EXPECT_GT(pos, 0);
    EXPECT_EQ(tag, ENDLOOP_TAG);
}

TEST_F(GetNextTagTest, NoLoopTagFound) {
    const char* data = "{if:condition} not a loop";
    int pos = getNextTag(data, strlen(data), LOOP_TAG_GROUP, &tag);

    EXPECT_EQ(pos, -1);
}

// Edge cases
TEST_F(GetNextTagTest, EmptyString) {
    const char* data = "";
    int pos = getNextTag(data, 0, IF_TAG_GROUP, &tag);

    EXPECT_EQ(pos, -1);
}

TEST_F(GetNextTagTest, OnlyOpenBrace) {
    const char* data = "{ not a tag";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_EQ(pos, -1);
}

TEST_F(GetNextTagTest, MultipleIfTags) {
    const char* data = "{if:a}{if:b}{endif}{endif}";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    // Should find the first {if: tag
    EXPECT_EQ(pos, 4);  // Position after "{if:"
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(GetNextTagTest, NestedTags) {
    const char* data = "{loop:items}{if:x}{endif}{endloop}";
    int pos = getNextTag(data, strlen(data), LOOP_TAG_GROUP, &tag);

    // Should find {loop:
    EXPECT_EQ(pos, 6);  // Position after "{loop:"
    EXPECT_EQ(tag, LOOP_TAG);
}

// Tests for find_tag_end_pos function
class FindTagEndPosTest : public ::testing::Test {};

TEST_F(FindTagEndPosTest, SimpleIfEndif) {
    const char* data = "content{endif}more";
    int pos = find_tag_end_pos(data, strlen(data), "{if:", "{endif}");

    // Should find the position of {endif}
    EXPECT_GE(pos, 0);
}

TEST_F(FindTagEndPosTest, NestedIfEndif) {
    const char* data = "{if:a}inner{endif}outer{endif}";
    int pos = find_tag_end_pos(data, strlen(data), "{if:", "{endif}");

    // Should find the matching outer {endif}
    EXPECT_GT(pos, 0);
}

TEST_F(FindTagEndPosTest, NoEndTag) {
    const char* data = "content without end tag";
    int pos = find_tag_end_pos(data, strlen(data), "{if:", "{endif}");

    EXPECT_EQ(pos, -1);
}

TEST_F(FindTagEndPosTest, LoopEndloop) {
    const char* data = "item{endloop}";
    int pos = find_tag_end_pos(data, strlen(data), "{loop:", "{endloop}");

    EXPECT_GE(pos, 0);
}

// Variable interpolation pattern tests (testing that braces work)
class VariablePatternTest : public ::testing::Test {};

TEST_F(VariablePatternTest, SimpleVariable) {
    // Test that stringfind can find variable patterns
    const char* data = "Hello {$name}!";
    int pos = stringfind(data, "{$");

    EXPECT_GT(pos, 0);
}

TEST_F(VariablePatternTest, MultipleVariables) {
    const char* data = "{$a} and {$b}";
    int pos = stringfind(data, "{$");

    EXPECT_GT(pos, 0);
}

TEST_F(VariablePatternTest, NoVariable) {
    const char* data = "plain text";
    int pos = stringfind(data, "{$");

    EXPECT_EQ(pos, 0);
}

// Tests for array variable patterns
TEST_F(VariablePatternTest, ArrayVariable) {
    const char* data = "Value: {$arr.key}";
    int pos = stringfind(data, "{$");

    EXPECT_GT(pos, 0);
}

TEST_F(VariablePatternTest, NestedArrayVariable) {
    const char* data = "{$obj.sub.value}";
    int pos = stringfind(data, "{$");

    // Should find at position 0+length of "{$"
    EXPECT_GT(pos, 0);
}

// Deeply Nested Structure Tests
class NestedTagTest : public ::testing::Test {
protected:
    TAG_IDS tag;
};

TEST_F(NestedTagTest, DeeplyNestedIf) {
    const char* data = "{if:a}{if:b}{if:c}inner{endif}{endif}{endif}";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_EQ(pos, 4);  // First {if:
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(NestedTagTest, NestedIfInLoop) {
    const char* data = "{loop:items}{if:active}show{endif}{endloop}";

    // Find loop first
    int pos = getNextTag(data, strlen(data), LOOP_TAG_GROUP, &tag);
    EXPECT_EQ(tag, LOOP_TAG);

    // Find if inside
    pos = getNextTag(data + pos, strlen(data) - pos, IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(NestedTagTest, LoopInIf) {
    const char* data = "{if:hasItems}{loop:items}item{endloop}{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(NestedTagTest, MultipleIfElse) {
    const char* data = "{if:a}first{else}second{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);

    // Find else
    pos = getNextTag(data + pos, strlen(data) - pos, IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, ELSE_TAG);
}

TEST_F(NestedTagTest, FindEndifAfterContent) {
    const char* data = "some content here {endif} more";
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);

    EXPECT_EQ(tag, ENDIF_TAG);
    EXPECT_GT(pos, 0);
}

// find_tag_end_pos with complex nesting
TEST_F(NestedTagTest, FindEndOfDeeplyNestedIf) {
    const char* data = "{if:b}{if:c}inner{endif}outer{endif}final";

    // Should find the outer endif
    int pos = find_tag_end_pos(data, strlen(data), "{if:", "{endif}");
    EXPECT_GT(pos, 0);
}

TEST_F(NestedTagTest, FindEndOfNestedLoop) {
    const char* data = "{loop:a}{loop:b}inner{endloop}outer{endloop}";

    int pos = find_tag_end_pos(data, strlen(data), "{loop:", "{endloop}");
    EXPECT_GT(pos, 0);
}

// Broken/Malformed Template Tests
class BrokenTemplateTest : public ::testing::Test {
protected:
    TAG_IDS tag;
};

TEST_F(BrokenTemplateTest, UnclosedIf) {
    const char* data = "{if:condition}content without endif";

    int pos = find_tag_end_pos(data, strlen(data), "{if:", "{endif}");
    EXPECT_EQ(pos, -1);  // Should not find endif
}

TEST_F(BrokenTemplateTest, UnclosedLoop) {
    const char* data = "{loop:items}content without endloop";

    int pos = find_tag_end_pos(data, strlen(data), "{loop:", "{endloop}");
    EXPECT_EQ(pos, -1);
}

TEST_F(BrokenTemplateTest, MismatchedTags) {
    const char* data = "{if:a}{loop:b}{endif}{endloop}";

    // This is malformed - if closes before loop
    // Parser should still find tags
    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(BrokenTemplateTest, EmptyTagName) {
    const char* data = "{if:}content{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(BrokenTemplateTest, OnlyOpenBrace) {
    const char* data = "{ incomplete tag";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(pos, -1);
}

TEST_F(BrokenTemplateTest, BraceWithoutTag) {
    const char* data = "{notavalidtag}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(pos, -1);
}

TEST_F(BrokenTemplateTest, PartialIfTag) {
    const char* data = "{if content";  // Missing colon

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(pos, -1);
}

TEST_F(BrokenTemplateTest, ExtraEndif) {
    const char* data = "{endif}{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, ENDIF_TAG);
}

TEST_F(BrokenTemplateTest, EndifWithoutIf) {
    const char* data = "content{endif}more";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, ENDIF_TAG);
}

// Edge cases in tag parsing
class TagEdgeCaseTest : public ::testing::Test {
protected:
    TAG_IDS tag;
};

TEST_F(TagEdgeCaseTest, TagAtVeryEnd) {
    const char* data = "content{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, ENDIF_TAG);
}

TEST_F(TagEdgeCaseTest, MultipleConsecutiveTags) {
    const char* data = "{if:a}{if:b}{if:c}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(pos, 4);  // After first {if:
}

TEST_F(TagEdgeCaseTest, TagWithSpecialCharsInCondition) {
    const char* data = "{if:var.sub[0].value}content{endif}";

    int pos = getNextTag(data, strlen(data), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}

TEST_F(TagEdgeCaseTest, VeryLongCondition) {
    std::string data = "{if:";
    for (int i = 0; i < 1000; i++) {
        data += "x";
    }
    data += "}content{endif}";

    int pos = getNextTag(data.c_str(), data.length(), IF_TAG_GROUP, &tag);
    EXPECT_EQ(tag, IF_TAG);
}
