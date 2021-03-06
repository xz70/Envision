/***********************************************************************************************************************
**
** Copyright (c) 2011, 2014 ETH Zurich
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
** following conditions are met:
**
**    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
**      disclaimer.
**    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
**      following disclaimer in the documentation and/or other materials provided with the distribution.
**    * Neither the name of the ETH Zurich nor the names of its contributors may be used to endorse or promote products
**      derived from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***********************************************************************************************************************/

#pragma once

#include "selftest_api.h"

#include "TestBase.h"
#include "TestManager.h"
#include "TestResults.h"

#include "Core/src/reflect/Reflect.h"

namespace SelfTest {

/**
 * The Test template class is used to define a new test.
 *
 * Purposefully outside of the namespace for easy access
 *
 * @param pluginClass
 * 				the name of the class of the current plugin. This is the class that implements the EnvisionPlugin
 * 				interface.
 * @param testName
 * 				the name of this test. This name can be used at the command line to request that this specific test be
 * 				started.
 */
template <typename PluginClass, typename TestName>
class Test : public TestBase {
	public:
		virtual void runCustom(TestResults& testResults, bool &allChecksPassedFlag) override;

	protected:
		TestResults testResults;
		bool allChecksPassedFlag{true};
		static bool initTrigger;
		Test();

	private:
		static bool init();
};

template<typename PluginClass, typename TestName>
inline void Test<PluginClass, TestName>::runCustom(TestResults& testResults, bool& allChecksPassedFlag)
{
	this->allChecksPassedFlag = true;
	this->testResults = {};

	static_cast<TestName*>(this)->test();
	testResults.merge(this->testResults);
	allChecksPassedFlag = this->allChecksPassedFlag;
}

template<typename PluginClass, typename TestName>
inline Test<PluginClass, TestName>::Test() : TestBase{typeName<TestName>().className_}
{
}

template<typename PluginClass, typename TestName>
inline bool Test<PluginClass, TestName>::init()
{
	TestManager<PluginClass>::add([](){return new TestName{};}, typeName<TestName>().className_);
	return true;
}

template <typename PluginClass, typename TestName>
bool Test<PluginClass, TestName>::initTrigger = Test::init();

}
