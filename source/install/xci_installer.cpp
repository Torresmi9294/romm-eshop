#include "install/xci_installer.hpp"
#include "install/install_progress_state.hpp"

#include <memory>
#include <stdexcept>

#include "install/install_xci.hpp"
#include "install/sdmc_xci.hpp"
#include "util/util.hpp"

namespace romm::install
{
    InstallResult InstallXci(const std::string& xciPath, InstallDestination destination)
    {
        InstallResult result;
        auto& progress = InstallProgressState::Instance();
        progress.Reset();

        NcmStorageId storageId = destination == InstallDestination::SdCard
                                      ? NcmStorageId_SdCard
                                      : NcmStorageId_BuiltInUser;

        inst::util::initInstallServices();

        try
        {
            progress.SetStatusText("Preparing installation...");

            auto sdmcXci = std::make_shared<tin::install::xci::SDMCXCI>(xciPath);
            tin::install::xci::XCIInstallTask installTask(storageId, /*ignoreReqFirmVersion=*/false, sdmcXci);

            installTask.Prepare();
            installTask.Begin();

            if (progress.GetVerificationFailed())
            {
                result.success = false;
                result.errorMessage = "Installation aborted: " + progress.GetStatusText();
            }
            else
            {
                progress.SetStatusText("Installation complete");
                progress.SetPercent(100);
                result.success = true;
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.errorMessage = e.what();
            progress.SetStatusText(std::string("Install failed: ") + e.what());
        }
        catch (...)
        {
            result.success = false;
            result.errorMessage = "Unknown install error";
            progress.SetStatusText("Install failed: unknown error");
        }

        inst::util::deinitInstallServices();
        return result;
    }
}
