#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"
#include "mfx_serializer.h"

namespace vpp_composition
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP(false)
        , m_sts(MFX_ERR_UNKNOWN)
    {
    }
    ~TestSuite()
    {
        CleanExtParam();
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxStatus   m_sts;

    void SetParam(std::string function)
    {
        CleanExtParam();
        DeSerializeFromYaml(m_sts, "mfxStatus." + function, g_tsConfig.cfg_filename);
        DeSerializeFromYaml(*m_pPar, "mfxVideoParam." + function, g_tsConfig.cfg_filename);
    }

    void CleanExtParam()
    {
        for (int i = 0; i < m_par.NumExtParam; i++)
        {
            if (m_par.ExtParam[i])
            {
                delete[] m_par.ExtParam[i];
                m_par.ExtParam[i] = 0;
            }
        }

        if (m_par.ExtParam)
        {
            delete[] m_par.ExtParam;
            m_par.ExtParam = 0;
        }

        m_par.NumExtParam = 0;
    }
};

const unsigned int TestSuite::n_cases = 1;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    MFXInit();

    SetParam("Init"); g_tsStatus.expect(m_sts);
    mfxStatus sts = Init(m_session, m_pPar);

    SetParam("Reset"); g_tsStatus.expect(m_sts);
    sts = Reset(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_composition);

}
