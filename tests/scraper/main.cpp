#include <gtest/gtest.h>
#include <QApplication>

int main(int argc, char** argv)
{
    // QApplication must be constructed before InitGoogleTest so Qt processes
    // --platform and similar args first.
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
