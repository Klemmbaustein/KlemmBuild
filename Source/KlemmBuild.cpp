#include "BuildSystem/VCBuild.h"

int main()
{
	BuildInfo Build;
	Build.TargetName = "MyTest";
	VCBuild TestBuild;

	TestBuild.PreprocessFile("Test/Source/test.cpp", {});

	TestBuild.Link({ TestBuild.Compile("Test/Source/test.cpp", &Build), TestBuild.Compile("Test/Source/test2.cpp", &Build) }, &Build);
}