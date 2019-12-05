MSDK Unit Testing Guide
============

Contacts: Dmitry Gurulev (dmitry.gurulev\@intel.com)

# General information

Unit test(s) (here and after UT(s)) is widely adopted as essential part of development process. The main goal of UTs is to prove the code actually works as expected and thus improve software quality.
UTs is a way to deliver software faster but without sacrificing its quality. UTs lies at the foundation of [Test Pyramid](https://martinfowler.com/bliki/TestPyramid.html) and are a basement of software quality, they directly interact with the code and they're capable to catch problems just near real root cause.
Additional benefits UTs provide are -
* Based on behavior of certain unit and therefore provides a fixed contract for it
* Allows code to be refactored, ensures the unit still behaves properly
* Detects changes that could break contracts at earlier stage, in the development phase (low level regression testing)
* Creates a sort of **actual** documentation and sample code for _Unit_ interface and its behavior
* Reveals design problems, compels developers to think about code ‘testability’ and thus improve _Unit_ design
* Reduces cost of bug fixing because narrows scope of suspected units

This document introduces basic principles of UTs design, gives a guide which code need to be tested, how and why, highlights common issues.

# Table of contents
* [Definitions](#definitions)
* [F.I.R.S.T.](#first)
* [Isolation](#isolation)
* [Scope](#scope)
* [Audit](#audit)
* [How to test](#how-to-test-test-case-design)
* [Best practices](#best-practices)
* [Step-by-Step sample](#step-by-step-sample)

# Definitions

## Common properties
* UTs are written and run by software developers
* UTs ensures that a part of an application (known as the _Unit_) meets its design and behaves as it is intended, focuses on one thing
* It is expected that UTs have to be **significantly faster** than other kinds of tests

## _Unit_ is ...<a name="unit-is"></a>

>But really it's a situational thing - the team decides what makes sense to be a unit for the purposes of their understanding of the system and its testing.
(c) Fowler M. [UnitTest](https://martinfowler.com/bliki/UnitTest.html)

In practice it is an atomic pieces of your program (code) -
* A function or method
* A class (interface)
* A set of classes/methods (closely related)
* A subset of class’ methods
* An architectural item (block) - a set of functions/methods/states somehow united (acts) as the one entity (via some `queue` for example)

# F.I.R.S.T.<a name="first"></a>
**F**ast **I**ndependent **R**epeatability **S**elf-Validating **T**imely
* **F** – Should be fast. Developers don’t run them if they take minutes
* **I** – Shouldn’t dependent on other test cases (order-of-run dependency).
* **R** – Same results every time at any environment
* **S** – Only boolean value FAIL or PASS, no manual interpretation
* **T** – Two interpretations ([TDD](https://en.wikipedia.org/wiki/Test-driven_development)'s and more common)
    - Code and test are written at same time (or test even before the code)
    - Time to write test shouldn’t take more time than writing the code

# Isolation

To make UTs results being the same at any environment ([F.I.R.S.T.](#first) principle's letter 'R') every undetermined external reference need to be replaced with special object that has
well determined behavior. These objects are denoted either as 'Fakes (or Stubs)' and 'Mocks'. The latter is used when tested object should call methods (in proper order and with correct arguments) of the external dependency.

Do we need to isolate ALL external dependencies? No, we don't need to isolate external dependencies that are stable (deterministic) and fast enough. Trying to mock everything provokes UTs
to be over complexed, fragile and obstructs test cases understanding. So, such dependencies as helpers or computational classes could be used `as-is`, but of course should also be covered by dedicated UTs in turn.
We may state, that the real border where these dependencies have to be isolated is the external **system**.

# Scope

## What to test and what not

First of all, a new code you write is required to be covered with UTs if it
* implements a new feature
* causes functional coverage less than 80%
* reduces condition/decision coverage more than 0.5%

Second, it is recommended to cover
* The code which is frequently changed (we can identify this by using code churn metric, **TBD**)
* The code which is identified as being complex - [Cyclomatic complexity](https://en.wikipedia.org/wiki/Cyclomatic_complexity) is over 5
* The code which is shared across other components (we can identify this by using _Fan-In_ procedures metric, **TBD**)

It's nice to have tests for old legacy code (especially when you're going to change it - a subject for planned refactoring), but it's not required. Also, if a bug was found it's better to write UT that reproduce the bug
and then fix it to make UT pass. With that approach we continuously improve coverage and have tests to track regressions.

Third, the only code that could break have to be tested. It's not needed to test
* Trivial classes/functions, like simple facade/wrappers or getters/setters
* Constructors, unless they don't contain conditions and don't provide result status in any form - exceptions/out parameters/methods like `IsInitialized`.
* Stable legacy code - that's a subject of functional/integration/regression testing
* 3d-party code (libraries)

The only public interface (`public` methods and non-static functions) need to be tested, other things are part of implementation. In case you covered private methods with UTs,
you have to fix UTs every time you change implementation, even if **behavior** is not changed! So, usually ([*](#usially-not-test)) don't need to be tested
* Parent's class behavior at UTs of derived class
* `private`/`protected` methods or static functions directly, they have to be tested through public interface
* Exception's/Log's or Trace messages, if they are not a part of public contract
* Multi-threading code because of its indetermenistic by nature, better test it by functional tests or with help of dedicated analyzing tools ([Thread Sanitizer](https://en.wikipedia.org/wiki/Valgrind#Other_tools), [Intel Inspector](https://software.intel.com/en-us/inspector))

(*)<a name="usially-not-test"></a> As always in software development, that's not a set-in-stone rule, but rather subject of engineer's judgement.

## Audit

The main target is improving use case (specification) and formal code metrics coverage. General considerations are:
* Avoid threshold less than 80%
* For condition/decision coverage
    - More than 90% is not essentially lead to significant quality improvement
    - Trying to get more then 90% may consume to much efforts

So, recommended audit thresholds are:
* New feature's functional coverage is 80%
* Condition/decision coverage is 90%
* Any change that causes coverage drop more than 5% (functional coverage) or 0.5% (condition/decision coverage)

There is no formal metric for use case (specification), so it's subject of code-review.

# How to test: test case design

Test case is an entity that describes input, action and a result which describes developer's expectation if unit works properly or not.
In the most cases, this is a heuristic approach to design UTs, it's hard to define strict rules. Anyway, the more or less common approach is:
* Define a _Unit_ - which piece of code you have to cover with UT
* Define an input for the UT
* Define expectations (output).
* Check whether _Unit_ produces the expected result

## How to define _Unit_

The answer is [_Unit_ is ...](#unit-is) and [UT scope](#scope)

## How to define input

The answer is completely depend on the _Unit_ being tested, so input is defined basing on engineers’ understanding of the sources like:
* MSDK API requirements, referring to [MSDK manual](../api/documentation/mediasdk-man.md) (and other documentation at the (../api/documentation) folder)
* System level APIs requirements - Win32 [DrectX](https://docs.microsoft.com/en-us/windows/win32/api/d3d11)/[DXVA](https://docs.microsoft.com/en-us/windows/win32/api/dxva2api) or [libVA](https://github.com/intel/libva)
* Components' specific [DDI](https://sharepoint.amr.ith.intel.com/sites/VPG_SW_Media/MediaSWuArch/Shared%20Documents/DDI) requirements
* [Intel's Graphics Documentation](https://gfxspecs.intel.com/Predator/Home)
* Components' standard requirements, like [ITU-T H.265](https://www.itu.int/ITU-T/recommendations/rec.aspx?rec=13904&lang=en) (International Standard ISO/IEC 23008-2) recommendations

There are the following essential parts of UT that define the input:
* Positive cases (happy path) – [preconditions](https://en.wikipedia.org/wiki/Precondition) are satisfied
* Corner cases (boundary values) – special attention to values near the limit
* Negative cases (supposed to fail) – preconditions' violation

Input is not only function/method arguments, but also _Unit_'s state - both internal state and state of its dependencies, real or mocked.
To prepare input for UT parts listed above, we can use several approaches - black box (data driving) or white box (code/logic driving).

### Black box
The first one consider _Unit_ (code) as unknown black box and the only thing we know is its behavior, type of input parameters and initial state. To derive input vectors at black box approach, we usually
* Split the whole domain of input data into three categories [(equivalence partitions)](https://en.wikipedia.org/wiki/Equivalence_partitioning) in such way,
that all inputs inside each category are equivalent, in terms of exposed _Unit_ behavior (requirements' specification). In other words, that's enough to cover each partition with test cases
just once: if one input passes - all others from this partition also pass, if one fails - all others also fail. This greatly reduces the total number of test cases and, therefore, the overall time required for testing.
Please keep in mind the main properties of these partitions:
    - None of inputs can present in different partitions (inputs can't be shared between partitions)
    - None of partitions can be empty
* Perform boundary-value analysis. Boundary values are the values that are on the edge of an equivalence partition or at the smallest distance +/- from the edge.
Typically these are min/max of the range, min/max +/- 1, near the middle (zero). Sometimes this values can't be identified, for example when members of the equivalence partition are not somehow ordered.
* Perform error guessing analysis. Consider such inputs as blank/null input, out of range, signed/unsigned, zero, empty strings, already sorted (or unsorted) sequences etc.

In some cases, it makes sense to check full range of input values (blanket test coverage). For example, if we have the input as enumeration, like
```C
enum
{
    CHROMA_SUBSAMPLING_400,
    CHROMA_SUBSAMPLING_420,
    CHROMA_SUBSAMPLING_422,
    CHROMA_SUBSAMPLING_444
};
```
it makes sense to cover all subsampling values with test cases. Obviously, trying to cover full `int` range (or even `char`) doesn't.

### White box
The second approach is white box, which is based on formal metrics, such as
* Statement coverage – every statement in the code is executed
* Edge (path) coverage – every edge of the Control Flow graph is executed
* Decision (condition, modified condition/decision) coverage – test every decision (condition)
* Function coverage – each function in the Unit’s chain need to be invoked.

We use both condition/decision and function coverage metric to measure code coverage.

### [Grey box](https://en.wikipedia.org/wiki/Grey_box_testing)
That's obviously, a combination of white-box and black-box testing.

The main benefit of code coverage approach is that it gives the formal metrics and therefore can be measured by tools and be a formal acceptance criteria or progress indicator. The main drawback,
in turn, is the test that reveals a problem will never be written if it's based on the code that contains the problem. In other words, if the code doesn't contain required branch - you may have
100% condition coverage but problem (code doesn't work as it should) will be still here.

## How to define expectations (outputs)

There are the following outputs:
* Method(function)'s return value(s) or output parameters
* Status (error) code
* Exception
* State change ([State](#state-verification))
* Interaction with external dependencies ([Behavior](#behavior-verification))

The best way to check first three of above is to have pre-calculated (somehow, for example using 3d-party tools) value(s). Do not try to reproduce algorithm of generating output in the test case.

### State verification
State verification means that we check whether the _Unit_ works correctly by examining its state, after the action is performed.
```C
struct Storage
{
    void Insert(key, value);
    bool Contains(key);
private:
    std::map<key, value> map;
};

TEST(Storage, InsertOk)
{
    Storage stg;
    ASSERT_FALSE(stg.Contains(key)); //precondition

    Stg.Insert(key, value);          //action
    EXPECT_TRUE(stg.Contains(key));  //verification
}
```
The main thing here is we need somehow get an access to the _Unit_ state. When this state is `protected` we can define a derived class and either bring this state into `public` (via [`using` declaration](https://en.cppreference.com/w/cpp/language/using_declaration))
or do more sophisticated (and potentially [UB](https://en.cppreference.com/w/cpp/language/ub)) tricks like
* Re-defining `private` as `public` keyword for UTs. That's essentially claims a special build configuration.
* Define a class w/ fully compatible layout and then use C-style cast to get an access to state via this new define class

All tricks above are dangerous and not recommended to use. More convenient approaches are
* Re-design _Unit_ to have the state to be injected ([DI](https://en.wikipedia.org/wiki/Dependency_injection) pattern) and therefore allow to either examine its state directly
or to mock it and verify the _Unit_ interacts with this dependency correctly (see (#behavior-verification) below)
* Cross-check the state through other available public methods of the _Unit_. In example above we use `bool Contains(key)` method for this purpose. The main thing here is that we have to be somehow confident
that `bool Contains(key)` works, in turn, correctly. This may lead to circular dependencies when we're able to check `void Insert(key, value)` through `bool Contains(key)` and vice versa.
The trick to break the loop is to choose one method that
    - could be verified through initial _Unit_ state (initial state)
    - has the simplest (in term of implementation, need to revise source code) and obvious implementation
and then incrementally check all other methods. Example:
    ```C
    struct Storage
    {
        void Insert(key, value);
        bool Contains(key);
        bool Empty()
        { return map.empty(); }

    private:
        std::map<key, value> map;
    }

    TEST(Storage, ContainsOk)
    {
        Storage stg;
        stg.Insert(key, value);
        EXPECT_TRUE(stg.Contains(key));
    }

    TEST(Storage, ContainsNoKey)
    {
        Storage stg;
        ASSERT_TRUE(stg.Empty());
        EXPECT_FALSE(stg.Contains(key));
    }
    ```

### Behavior verification
Behavior verification is a method to verify _Unit_ works as intended by checking its interaction with its external dependencies. This method requires that object dependencies could be mocked,
for example with help of dedicated framework ([googlemock](https://github.com/google/googletest/tree/master/googlemock)). Example:
```C
struct Storage : IStorable
{
    void Erase(key); //removes value from map & deletes object
};
struct Value {
    ~value() { OnDestruct(); }
    MOCK_CONST_METHOD0(OnDestruct, void());
};
IStorable* v = new Value();
Storage stg(key, v);

//setup expectation - [Storage] have to delete object instance and therefore invoke its destructor and, in turn, [OnDestruct]
EXPECT_CALL(v, OnDestruct()).WillOnce(testing::Return());
storage.Erase(key);
```

# Best practices
* Name should clearly describe what a test case. Good enough is `Suite + Member + Case(Scenario/Condition) + Expectation`, example `TEST(Allocator, AllocateZeroBytesFails)`
* One expectation (assert) per case. This doesn't literally mean **one** assert, but all used assertions should be related to testing of the same action. Consider this -
```C
TEST(FeatureQMatrix, UpdateSPS)
{
    auto block = FeatureBlocks::Get(queue, { Gen11::FEATURE_QMATRIX, Windows::Gen11::QMatrix::BLK_UpdateSPS });
    auto& sps = Gen11::Glob::SPS::Get(storage);

    //test case precondition
    ASSERT_EQ(sps.scaling_list_enabled_flag, 0);
    ASSERT_EQ(sps.scaling_list_data_present_flag, 0);

    //verify expectation
    EXPECT_EQ(block->Call(storage, storage), MFX_ERR_NONE);
    EXPECT_EQ(sps.scaling_list_enabled_flag, 1);
    EXPECT_EQ(sps.scaling_list_data_present_flag, 1);
}
```
Here we have two `ASSERT_EQ` (that's [googlemock](https://github.com/google/googletest/tree/master/googlemock) concept that means that test execution will stop if assert condition is violated).
The intention is to have test case preconditions - UT has no sense at all if these conditions are violated. Other expectations are actually doing the work verifying _Unit_ behavior and there is no
sense to check method's return value and these two SPS's flags in separate test cases, because they are logically one entity.
* AAA principle<a name="aaa-principle"></a> – *A*rrange (setup) - *A*ct (invoke, capture result) and *A*ssert (check expectations). It means that better way is to firstly setup _Unit_ (prepare mocks, input data etc.),
then perform an action (invoke function/method of _Unit_) and then verify expectations. Following this principle facilitates reading and therefore understanding the test case intention.
* Separate setup (teardown) from test code. Sometimes it's even better to move test case setup (first letter `A` in AAA principle) away from test code, into separate entity (function/method).
The main purpose is same - improve test code readability, the second - if multiple test cases use object that requires same (or similar) setup it makes sense to extract it just to avoid duplication and copy/paste.
[googlemock](https://github.com/google/googletest/tree/master/googlemock) has a dedicated concept ([test fixture class](https://github.com/google/googletest/blob/master/googletest/docs/primer.md#test-fixtures-using-the-same-data-configuration-for-multiple-tests-same-data-multiple-tests)) for that purpose.
* Use special tools to measure code coverage. Example - [Bullseye](https://wiki.ith.intel.com/display/SoftwareEngineeringProcess/Bullseye+Code+Coverage)

# Step-by-Step sample

Let's create a test suite for one of MSDK's HEVC encoder's feature - quantization matrix support (`HEVCEHW::Gen11::QMatrix`).
Main architectural blocks of the encoder are `queue`, `feature` and `feature block`, and the latter is doing actual work. The artifacts that produces this feature are updating _SPS_ and picture parameters data (`D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA`) that is
part of DDI. So we may consider `feature block` entities as _Unit_s here - one is `BLK_UpdateSPS` and another is `BLK_UpdateDDISubmit`. What is intended behavior of these blocks?
* Works for `ICL`+, do nothing for platforms < `ICL`
* Applicable only for
    - [`MFX_SCENARIO_GAME_STREAMING`](../api/documentation/mediasdk-man.md#ScenarioInfo) scenario or
    - [`MFX_SCENARIO_REMOTE_GAMING`](../api/documentation/mediasdk-man.md#ScenarioInfo) and [`mfxInfoMFX :: LowPower`](../api/documentation/mediasdk-man.md#mfxInfoMFX) is On
    - do nothing otherwise
* Updates `SPS :: scaling_list_data_present_flag` and `SPS :: scaling_list_enabled_flag` (sets equal to one)
* Prepares `SPS :: scl.scalingListsX` arrays with default values (accord. to H.265 ITU specification) in correct scan order
* Fills `DXVA_Qmatrix_HEVC` matrix (part of DDI)
* Submits DDI's data into UMD through system API (`DXVA`)

Let's define input for `BLK_UpdateSPS` block. First of all, this is `mfxVideoParam` and particularly, [`mfxExtCodingOption3 :: ScenarioInfo`](../api/documentation/mediasdk-man.md#ScenarioInfo) plus [`mfxInfoMFX :: LowPower`](../api/documentation/mediasdk-man.md#mfxInfoMFX) parameters.
Formally, assuming that [`MFX_SCENARIO_GAME_STREAMING`](../api/documentation/mediasdk-man.md#ScenarioInfo) is just an integer number and using [(equivalence partitions)](https://en.wikipedia.org/wiki/Equivalence_partitioning) approach, we can define that

| inputs | valid | invalid 1 | invalid 2 |
| ------ | ----- | --------- | --------- |
| `ScenarioInfo`, *si* | *si* = `MFX_SCENARIO_GAME_STREAMING` or *si* = `MFX_SCENARIO_REMOTE_GAMING` & IsOn(LowPower) | *si* < `MFX_SCENARIO_GAME_STREAMING`| *si* > `MFX_SCENARIO_REMOTE_GAMING` |

That's good enough and we may proceed in that way, and even may amend cases with boundary analysis or even use blanket test coverage to check full range of [`ScenarioInfo`](../api/documentation/mediasdk-man.md#ScenarioInfo)
In practice, and since we know that [`ScenarioInfo`](../api/documentation/mediasdk-man.md#ScenarioInfo) is not an actually the range, but an enumeration, where values are not semantically (in term of domain area) ordered,
we may define just two inputs:
* valid - *ScenarioInfo* = `MFX_SCENARIO_GAME_STREAMING`
* valid - *ScenarioInfo* = `MFX_SCENARIO_REMOTE_GAMING` & `IsOn(LowPower))`
* not valid - any other value(s) from [`ScenarioInfo`](../api/documentation/mediasdk-man.md#ScenarioInfo) enumeration

Then, HW on which this block being executed is also the input. Using the same approach as above, derive the input as:
* valid *HW* >= `ICL` and
* not valid - *HW* < `ICL`

Let's apply error-guessing to amend our test cases coverage. The border values would be:
* [`MFX_SCENARIO_UNKNOWN`](../api/documentation/mediasdk-man.md#ScenarioInfo)
* [`mfxInfoMFX :: LowPower`](../api/documentation/mediasdk-man.md#mfxInfoMFX) is not a boolean but `mfxU16`, so it makes sense to test also `MFX_CODINGOPTION_UNKNOWN`
* `MFX_HW_UNKNOWN`

Now let's define [Arrange part](#aaa-principle) for `BLK_UpdateSPS` block. We need _Unit_ itself, `mfxInfoMFX` and `mfxExtCodingOption3`, so we use something like
```C
//test case verifies that neither SPS nor D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA is modified
TEST(QMatrix, UpdateSPSScenarioNotApplicable)
{
    auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
    auto block = FeatureBlocks::Get(queue, { Gen11::FEATURE_QMATRIX, Windows::Gen11::QMatrix::BLK_UpdateSPS });

    auto& vp  = Gen11::Glob::VideoParam::Get(storage);
    vp.NewEB(MFX_EXTBUFF_CODING_OPTION3);
    //set ScenarioInfo
    mfxExtCodingOption3& co3  = ExtBuffer::Get(vp);
    co3.ScenarioInfo = MFX_SCENARIO_UNKNOWN;

    //set SPS
    auto& sps = Gen11::Glob::SPS::Get(storage);
    sps.scaling_list_data_present_flag = 0;
    sps.scaling_list_enabled_flag      = 0;
}
```

But how do we setup HW? This is an internal state of _Unit_ dependency - MFX runtime (`core`), so to do that we need to somehow substitute it with `fake` object that gives us required behavior.
For example with help of newly `mock` library it could be done in this way -
```C
{
    mfxVideoParam const& vp  = Gen11::Glob::VideoParam::Get(storage);
    auto device = mocks::mfx::dx11::make_encoder(nullptr, nullptr, //don't care about certain adapter and context
        mocks::mfx::HW_SKL,
        std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp) //support this GUID
    );
}
```

[Act part](#aaa-principle) quite simple, all we need just invoke `Call` method of our block
```C
EXPECT_EQ(
    block->Call(storage, storage),
    MFX_ERR_NONE
);
```

For [Assert part](#aaa-principle), we expect that when [`ScenarioInfo`](../api/documentation/mediasdk-man.md#ScenarioInfo)
* in valid range, the `SPS :: scaling_list_data_present_flag` and `SPS :: scaling_list_enabled_flag` is set
```C
EXPECT_NE(
    sps.scaling_list_enabled_flag, 1
);
EXPECT_NE(
    sps.scaling_list_data_present_flag, 1
);
```
* not in valid range, the `SPS :: scaling_list_data_present_flag` and `SPS :: scaling_list_enabled_flag` is not set
```C
EXPECT_EQ(
    sps.scaling_list_enabled_flag, 1
);
EXPECT_EQ(
    sps.scaling_list_data_present_flag, 1
);
```

Now you may understand that several tests cases looks very similar to each other, espesially in its [Arrange part](#aaa-principle).
It might be improved by using [fixture class](https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite):
```C
struct QMatrix : testing::Test
{
    void SetUp() override
    {
       auto& vp  = Gen11::Glob::VideoParam::Get(storage);
       vp.NewEB(MFX_EXTBUFF_CODING_OPTION3);
       mfxExtCodingOption3& co3  = ExtBuffer::Get(vp);
       //set ScenarioInfo

       //set SPS
       auto& sps = Gen11::Glob::SPS::Get(storage);

       //setup initial state
       device = mocks::mfx::dx11::make_encoder(nullptr, nullptr, //don't care about certain adapter and context
           mocks::mfx::HW_SKL,
           std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp) //support this GUID
       );
    }
};

TEST_F(QMatrix, UpdateSPSScenarioNotApplicable)
{
    //rest of AAA here
}
```
You may note, that with [fixture class](https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite) we also use
yet another clause from [Best practices](#best-practices) list - separate setup (teardown) from test code.

But what about other inputs/expectations? To cover them we still need to write very similar code... The answer is [Value-Parameterized tests](https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#value-parameterized-tests):
```C
struct Qmatrix
: testing::TestWithParam<std::tuple<
    int /* ScenarioInfo */,
    int /* initial SPS flag */,
    int /* expected SPS flag */
>
{
    void SetUp() override
    {
        //set ScenarioInfo
        co3.ScenarioInfo = GetParam();

        //set SPS
        sps.scaling_list_data_present_flag = std::get<1>(GetParam());
    }
};

INSTANTIATE_TEST_SUITE_P(QmatrixScenario, Qmatrix,
    testing::Values(
        std::make_tuple(MFX_SCENARIO_UNKNOWN, 0),
        std::make_tuple(MFX_SCENARIO_GAME_STREAMING, 1)
    )
);

TEST_F(QMatrix, UpdateSPSScenario)
{
    EXPECT_EQ(sps.scaling_list_data_present_flag, std::get<2>(GetParam())
);
```

