
#include "application/application_facade.h"
#include "application/ip_access_policy_service.h"
#include "infrastructure/network/qt_ip_access_policy_repository.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "viewer/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Dynamic LAR System"));
    QApplication::setOrganizationName(QStringLiteral("LAR Engineering"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // UI-thread state and the production worker runtime.
    ModeCoordinator modeCoordinator;
    ApplicationViewModel viewModel;
    ThreadedApplicationRuntime runtime;
    QtIpAccessPolicyRepository ipAccessPolicyRepository;
    IpAccessPolicyService ipAccessPolicyService(ipAccessPolicyRepository);

    ApplicationFacade facade(modeCoordinator, runtime, viewModel, ipAccessPolicyService);

    // Presentation Window
    MainWindow window(facade);
    window.show();
    return application.exec();
}
