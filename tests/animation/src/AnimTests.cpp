//
//  AnimTests.cpp
//
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimTests.h"
#include <AnimNodeLoader.h>
#include <AnimClip.h>
#include <AnimBlendLinear.h>
#include <AnimationLogging.h>
#include <AnimVariant.h>
#include <AnimExpression.h>
#include <AnimUtil.h>

#include <../QTestExtensions.h>

QTEST_MAIN(AnimTests)

const float EPSILON = 0.001f;

void AnimTests::initTestCase() {
    auto animationCache = DependencyManager::set<AnimationCache>();
    auto resourceCacheSharedItems = DependencyManager::set<ResourceCacheSharedItems>();
}

void AnimTests::cleanupTestCase() {
    DependencyManager::destroy<AnimationCache>();
}

void AnimTests::testClipInternalState() {
    QString id = "my anim clip";
    QString url = "https://hifi-public.s3.amazonaws.com/ozan/support/FightClubBotTest1/Animations/standard_idle.fbx";
    float startFrame = 2.0f;
    float endFrame = 20.0f;
    float timeScale = 1.1f;
    bool loopFlag = true;

    AnimClip clip(id, url, startFrame, endFrame, timeScale, loopFlag);

    QVERIFY(clip.getID() == id);
    QVERIFY(clip.getType() == AnimNode::Type::Clip);

    QVERIFY(clip._url == url);
    QVERIFY(clip._startFrame == startFrame);
    QVERIFY(clip._endFrame == endFrame);
    QVERIFY(clip._timeScale == timeScale);
    QVERIFY(clip._loopFlag == loopFlag);
}

static float framesToSec(float secs) {
    const float FRAMES_PER_SECOND = 30.0f;
    return secs / FRAMES_PER_SECOND;
}

void AnimTests::testClipEvaulate() {
    QString id = "myClipNode";
    QString url = "https://hifi-public.s3.amazonaws.com/ozan/support/FightClubBotTest1/Animations/standard_idle.fbx";
    float startFrame = 2.0f;
    float endFrame = 22.0f;
    float timeScale = 1.0f;
    float loopFlag = true;

    auto vars = AnimVariantMap();
    vars.set("FalseVar", false);

    AnimClip clip(id, url, startFrame, endFrame, timeScale, loopFlag);

    AnimNode::Triggers triggers;
    clip.evaluate(vars, framesToSec(10.0f), triggers);
    QCOMPARE_WITH_ABS_ERROR(clip._frame, 12.0f, EPSILON);

    // does it loop?
    triggers.clear();
    clip.evaluate(vars, framesToSec(12.0f), triggers);
    QCOMPARE_WITH_ABS_ERROR(clip._frame, 3.0f, EPSILON);  // Note: frame 3 and not 4, because extra frame between start and end.

    // did we receive a loop trigger?
    QVERIFY(std::find(triggers.begin(), triggers.end(), "myClipNodeOnLoop") != triggers.end());

    // does it pause at end?
    triggers.clear();
    clip.setLoopFlagVar("FalseVar");
    clip.evaluate(vars, framesToSec(20.0f), triggers);
    QCOMPARE_WITH_ABS_ERROR(clip._frame, 22.0f, EPSILON);

    // did we receive a done trigger?
    QVERIFY(std::find(triggers.begin(), triggers.end(), "myClipNodeOnDone") != triggers.end());
}

void AnimTests::testClipEvaulateWithVars() {
    QString id = "myClipNode";
    QString url = "https://hifi-public.s3.amazonaws.com/ozan/support/FightClubBotTest1/Animations/standard_idle.fbx";
    float startFrame = 2.0f;
    float endFrame = 22.0f;
    float timeScale = 1.0f;
    float loopFlag = true;

    float startFrame2 = 22.0f;
    float endFrame2 = 100.0f;
    float timeScale2 = 1.2f;
    bool loopFlag2 = false;

    auto vars = AnimVariantMap();
    vars.set("startFrame2", startFrame2);
    vars.set("endFrame2", endFrame2);
    vars.set("timeScale2", timeScale2);
    vars.set("loopFlag2", loopFlag2);

    AnimClip clip(id, url, startFrame, endFrame, timeScale, loopFlag);
    clip.setStartFrameVar("startFrame2");
    clip.setEndFrameVar("endFrame2");
    clip.setTimeScaleVar("timeScale2");
    clip.setLoopFlagVar("loopFlag2");

    AnimNode::Triggers triggers;
    clip.evaluate(vars, framesToSec(0.1f), triggers);

    // verify that the values from the AnimVariantMap made it into the clipNode's
    // internal state
    QVERIFY(clip._startFrame == startFrame2);
    QVERIFY(clip._endFrame == endFrame2);
    QVERIFY(clip._timeScale == timeScale2);
    QVERIFY(clip._loopFlag == loopFlag2);
}

void AnimTests::testLoader() {
    auto url = QUrl("https://gist.githubusercontent.com/hyperlogic/857129fe04567cbe670f/raw/0c54500f480fd7314a5aeb147c45a8a707edcc2e/test.json");
    // NOTE: This will warn about missing "test01.fbx", "test02.fbx", etc. if the resource loading code doesn't handle relative pathnames!
    // However, the test will proceed.
    AnimNodeLoader loader(url);

    const int timeout = 1000;
    QEventLoop loop;

    AnimNode::Pointer node = nullptr;
    connect(&loader, &AnimNodeLoader::success, [&](AnimNode::Pointer nodeIn) { node = nodeIn; });

    loop.connect(&loader, SIGNAL(success(AnimNode::Pointer)), SLOT(quit()));
    loop.connect(&loader, SIGNAL(error(int, QString)), SLOT(quit()));
    QTimer::singleShot(timeout, &loop, SLOT(quit()));

    loop.exec();

    QVERIFY((bool)node);

    QVERIFY(node->getID() == "blend");
    QVERIFY(node->getType() == AnimNode::Type::BlendLinear);

    QVERIFY((bool)node);
    QVERIFY(node->getID() == "blend");
    QVERIFY(node->getType() == AnimNode::Type::BlendLinear);

    auto blend = std::static_pointer_cast<AnimBlendLinear>(node);
    QVERIFY(blend->_alpha == 0.5f);

    QVERIFY(node->getChildCount() == 3);

    std::shared_ptr<AnimNode> nodes[3] = { node->getChild(0), node->getChild(1), node->getChild(2) };

    QVERIFY(nodes[0]->getID() == "test01");
    QVERIFY(nodes[0]->getChildCount() == 0);
    QVERIFY(nodes[1]->getID() == "test02");
    QVERIFY(nodes[1]->getChildCount() == 0);
    QVERIFY(nodes[2]->getID() == "test03");
    QVERIFY(nodes[2]->getChildCount() == 0);

    auto test01 = std::static_pointer_cast<AnimClip>(nodes[0]);
    QVERIFY(test01->_url == "test01.fbx");
    QVERIFY(test01->_startFrame == 1.0f);
    QVERIFY(test01->_endFrame == 20.0f);
    QVERIFY(test01->_timeScale == 1.0f);
    QVERIFY(test01->_loopFlag == false);

    auto test02 = std::static_pointer_cast<AnimClip>(nodes[1]);
    QVERIFY(test02->_url == "test02.fbx");
    QVERIFY(test02->_startFrame == 2.0f);
    QVERIFY(test02->_endFrame == 21.0f);
    QVERIFY(test02->_timeScale == 0.9f);
    QVERIFY(test02->_loopFlag == true);
}

void AnimTests::testVariant() {
    auto defaultVar = AnimVariant();
    auto boolVarTrue = AnimVariant(true);
    auto boolVarFalse = AnimVariant(false);
    auto intVarZero = AnimVariant(0);
    auto intVarOne = AnimVariant(1);
    auto intVarNegative = AnimVariant(-1);
    auto floatVarZero = AnimVariant(0.0f);
    auto floatVarOne = AnimVariant(1.0f);
    auto floatVarNegative = AnimVariant(-1.0f);
    auto vec3Var = AnimVariant(glm::vec3(1.0f, -2.0f, 3.0f));
    auto quatVar = AnimVariant(glm::quat(1.0f, 2.0f, -3.0f, 4.0f));
    auto mat4Var = AnimVariant(glm::mat4(glm::vec4(1.0f, 2.0f, 3.0f, 4.0f),
                                         glm::vec4(5.0f, 6.0f, -7.0f, 8.0f),
                                         glm::vec4(9.0f, 10.0f, 11.0f, 12.0f),
                                         glm::vec4(13.0f, 14.0f, 15.0f, 16.0f)));
    QVERIFY(defaultVar.isBool());
    QVERIFY(defaultVar.getBool() == false);

    QVERIFY(boolVarTrue.isBool());
    QVERIFY(boolVarTrue.getBool() == true);
    QVERIFY(boolVarFalse.isBool());
    QVERIFY(boolVarFalse.getBool() == false);

    QVERIFY(intVarZero.isInt());
    QVERIFY(intVarZero.getInt() == 0);
    QVERIFY(intVarOne.isInt());
    QVERIFY(intVarOne.getInt() == 1);
    QVERIFY(intVarNegative.isInt());
    QVERIFY(intVarNegative.getInt() == -1);

    QVERIFY(floatVarZero.isFloat());
    QVERIFY(floatVarZero.getFloat() == 0.0f);
    QVERIFY(floatVarOne.isFloat());
    QVERIFY(floatVarOne.getFloat() == 1.0f);
    QVERIFY(floatVarNegative.isFloat());
    QVERIFY(floatVarNegative.getFloat() == -1.0f);

    QVERIFY(vec3Var.isVec3());
    auto v = vec3Var.getVec3();
    QVERIFY(v.x == 1.0f);
    QVERIFY(v.y == -2.0f);
    QVERIFY(v.z == 3.0f);

    QVERIFY(quatVar.isQuat());
    auto q = quatVar.getQuat();
    QVERIFY(q.w == 1.0f);
    QVERIFY(q.x == 2.0f);
    QVERIFY(q.y == -3.0f);
    QVERIFY(q.z == 4.0f);

    QVERIFY(mat4Var.isMat4());
    auto m = mat4Var.getMat4();
    QVERIFY(m[0].x == 1.0f);
    QVERIFY(m[1].z == -7.0f);
    QVERIFY(m[3].w == 16.0f);
}

void AnimTests::testAccumulateTime() {

    float startFrame = 0.0f;
    float endFrame = 10.0f;
    float timeScale = 1.0f;
    testAccumulateTimeWithParameters(startFrame, endFrame, timeScale);

    startFrame = 5.0f;
    endFrame = 15.0f;
    timeScale = 1.0f;
    testAccumulateTimeWithParameters(startFrame, endFrame, timeScale);

    startFrame = 0.0f;
    endFrame = 10.0f;
    timeScale = 0.5f;
    testAccumulateTimeWithParameters(startFrame, endFrame, timeScale);

    startFrame = 5.0f;
    endFrame = 15.0f;
    timeScale = 2.0f;
    testAccumulateTimeWithParameters(startFrame, endFrame, timeScale);
}

void AnimTests::testAccumulateTimeWithParameters(float startFrame, float endFrame, float timeScale) const {

    float dt = (1.0f / 30.0f) / timeScale;  // sec
    QString id = "testNode";
    AnimNode::Triggers triggers;
    bool loopFlag = false;

    float resultFrame = accumulateTime(startFrame, endFrame, timeScale, startFrame, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == startFrame + 1.0f);
    QVERIFY(triggers.empty());
    triggers.clear();

    resultFrame = accumulateTime(startFrame, endFrame, timeScale, resultFrame, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == startFrame + 2.0f);
    QVERIFY(triggers.empty());
    triggers.clear();

    resultFrame = accumulateTime(startFrame, endFrame, timeScale, resultFrame, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == startFrame + 3.0f);
    QVERIFY(triggers.empty());
    triggers.clear();

    // test onDone trigger and frame clamping.
    resultFrame = accumulateTime(startFrame, endFrame, timeScale, endFrame - 1.0f, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == endFrame);
    QVERIFY(!triggers.empty() && triggers[0] == "testNodeOnDone");
    triggers.clear();

    resultFrame = accumulateTime(startFrame, endFrame, timeScale, endFrame - 0.5f, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == endFrame);
    QVERIFY(!triggers.empty() && triggers[0] == "testNodeOnDone");
    triggers.clear();

    // test onLoop trigger and looping frame logic
    loopFlag = true;

    // should NOT trigger loop even though we stop at last frame, because there is an extra frame between end and start frames.
    resultFrame = accumulateTime(startFrame, endFrame, timeScale, endFrame - 1.0f, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == endFrame);
    QVERIFY(triggers.empty());
    triggers.clear();

    // now we should hit loop trigger
    resultFrame = accumulateTime(startFrame, endFrame, timeScale, resultFrame, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == startFrame);
    QVERIFY(!triggers.empty() && triggers[0] == "testNodeOnLoop");
    triggers.clear();

    // should NOT trigger loop, even though we move past the end frame, because of extra frame between end and start.
    resultFrame = accumulateTime(startFrame, endFrame, timeScale, endFrame - 0.5f, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == endFrame + 0.5f);
    QVERIFY(triggers.empty());
    triggers.clear();

    // now we should hit loop trigger
    resultFrame = accumulateTime(startFrame, endFrame, timeScale, resultFrame, dt, loopFlag, id, triggers);
    QVERIFY(resultFrame == startFrame + 0.5f);
    QVERIFY(!triggers.empty() && triggers[0] == "testNodeOnLoop");
    triggers.clear();

void AnimTests::testTokenizer() {
    QString str = "(10 +  x) >= 20 && (y != !z)";
    AnimExpression e("");
    auto iter = str.cbegin();
    AnimExpression::Token token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::LeftParen);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::LiteralInt);
    QVERIFY(token.intVal == 10);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::Plus);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::Identifier);
    QVERIFY(token.strVal == "x");
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::RightParen);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::GreaterThanEqual);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::LiteralInt);
    QVERIFY(token.intVal == 20);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::And);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::LeftParen);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::Identifier);
    QVERIFY(token.strVal == "y");
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::NotEqual);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::Not);
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::Identifier);
    QVERIFY(token.strVal == "z");
    token = e.consumeToken(str, iter);
    QVERIFY(token.type == AnimExpression::Token::RightParen);
    token = e.consumeToken(str, iter);
}

