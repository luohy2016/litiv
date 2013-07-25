#include "BackgroundSubtractorLBSP.h"
#include "DatasetUtils.h"

//////////////////////////////////////////
// USER/ENVIRONMENT-SPECIFIC VARIABLES :
#define WRITE_ANALYSIS_RESULTS			0
#define DISPLAY_ANALYSIS_DEBUG_RESULTS	1
#define WRITE_ANALYSIS_DEBUG_RESULTS	0
#define USE_RELATIVE_LBSP_COMPARISONS	1
#define USE_CDNET_DATASET				1
#define USE_WALLFLOWER_DATASET			0
#define DATASET_ROOT_DIR 				std::string("/shared/datasets/")
//////////////////////////////////////////

#if !USE_CDNET_DATASET && !USE_WALLFLOWER_DATASET
#error "No dataset specified."
#elif USE_CDNET_DATASET && USE_WALLFLOWER_DATASET
#error "Multiple datasets specified."
#elif USE_CDNET_DATASET
const std::string g_sDatasetName("CDNet");
const std::string g_sDatasetPath(DATASET_ROOT_DIR+"CDNet/dataset/");
const std::string g_sResultsPath(DATASET_ROOT_DIR+"CDNet/results_test/");
const std::string g_sResultPrefix("bin");
const std::string g_sResultSuffix(".png");
const int g_nResultIdxOffset = 1;
#elif USE_WALLFLOWER_DATASET
const std::string g_sDatasetName("WALLFLOWER");
const std::string g_sDatasetPath(DATASET_ROOT_DIR+"Wallflower/dataset");
const std::string g_sResultsPath(DATASET_ROOT_DIR+"Wallflower/results_test/");
const std::string g_sResultPrefix("bin");
const std::string g_sResultSuffix(".png");
const int g_nResultIdxOffset = 0;
#endif

int AnalyzeSequence(int nThreadIdx, CategoryInfo* pCurrCategory, SequenceInfo* pCurrSequence);
cv::Mat GetDisplayResult(const cv::Mat& oInputImg, const cv::Mat& oBGImg, const cv::Mat& oBGDesc, const cv::Mat& oFGMask, std::vector<cv::KeyPoint> voKeyPoints, size_t nFrame);

#if WIN32 && _MSC_VER <= 1600 // no c++11 support
#define USE_WINDOWS_API
#include <windows.h>
#include <process.h>
const int g_anResultsComprParams[2] = {CV_IMWRITE_PNG_COMPRESSION,9}; // lower to increase processing speed
const std::vector<int> g_vnResultsComprParams(g_anResultsComprParams,g_anResultsComprParams+2);
const size_t g_nMaxThreads = 4;
HANDLE g_hThreadEvent[g_nMaxThreads] = {0};
HANDLE g_hThreads[g_nMaxThreads] = {0};
void* g_apThreadDataStruct[g_nMaxThreads][2] = {0};
DWORD WINAPI AnalyzeSequenceEntryPoint(LPVOID lpParam) {
	return AnalyzeSequence((int)(lpParam),(CategoryInfo*)g_apThreadDataStruct[(int)(lpParam)][0],(SequenceInfo*)g_apThreadDataStruct[(int)(lpParam)][1]);
}
#else //!WIN32 || _MSC_VER > 1600
#include <thread>
#include <chrono>
#include <atomic>
const std::vector<int> g_vnResultsComprParams = {CV_IMWRITE_PNG_COMPRESSION,9}; // lower to increase processing speed
const size_t g_nMaxThreads = (std::thread::hardware_concurrency()>0?std::thread::hardware_concurrency():1);
std::atomic_size_t g_nActiveThreads(0);
#endif //!WIN32 || _MSC_VER > 1600

///////////////////////////////////
int main( int argc, char** argv ) {
	srand(0); // for now, assures that two consecutive runs on the same data return the same results
	setvbuf(stdout, NULL, _IONBF, 0); // fixes output flush problems when using the eclipse built-in console
	std::vector<CategoryInfo*> vpCategories;
	std::cout << "Parsing dataset '"<< g_sDatasetName << "'..." << std::endl;
	try {
#if USE_CDNET_DATASET
		vpCategories.push_back(new CategoryInfo("baseline", g_sDatasetPath+"baseline", g_sDatasetName));
		vpCategories.push_back(new CategoryInfo("cameraJitter", g_sDatasetPath+"cameraJitter", g_sDatasetName));
		vpCategories.push_back(new CategoryInfo("dynamicBackground", g_sDatasetPath+"dynamicBackground", g_sDatasetName));
		vpCategories.push_back(new CategoryInfo("intermittentObjectMotion", g_sDatasetPath+"intermittentObjectMotion", g_sDatasetName));
		vpCategories.push_back(new CategoryInfo("shadow", g_sDatasetPath+"shadow", g_sDatasetName));
		vpCategories.push_back(new CategoryInfo("thermal", g_sDatasetPath+"thermal", g_sDatasetName));
#elif USE_WALLFLOWER_DATASET
		vpCategories.push_back(new CategoryInfo("global", g_sDatasetPath, g_sDatasetName));
#endif
	} catch(std::runtime_error& e) { std::cout << e.what() << std::endl; }
	size_t nSeqTotal = 0;
	for(auto pCurrCategory=vpCategories.begin(); pCurrCategory!=vpCategories.end(); ++pCurrCategory)
		nSeqTotal += (*pCurrCategory)->m_vpSequences.size();
	std::cout << "Parsing complete. [" << vpCategories.size() << " category(ies), "  << nSeqTotal  << " sequence(s)]" << std::endl << std::endl;
	if(nSeqTotal) {
		// since the algorithm isn't implemented to be parallelized yet, we parallelize the sequence treatment instead
		std::cout << "Running LBSP background subtraction with " << g_nMaxThreads << " thread(s)..." << std::endl;
		size_t nSeqProcessed = 1;
#ifndef USE_WINDOWS_API
		for(auto& pCurrCategory : vpCategories) {
			for(auto& pCurrSequence : pCurrCategory->m_vpSequences) {
				while(g_nActiveThreads>=g_nMaxThreads)
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				std::cout << "\tProcessing sequence " << nSeqProcessed << "/" << nSeqTotal << "... (" << pCurrCategory->m_sName << ":" << pCurrSequence->m_sName << ")" << std::endl;
				g_nActiveThreads++;
				nSeqProcessed++;
				std::thread(AnalyzeSequence,-1,pCurrCategory,pCurrSequence).detach();
			}
		}
		while(g_nActiveThreads>0)
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#else //USE_WINDOWS_API
		for(size_t n=0; n<g_nMaxThreads; ++n)
			g_hThreadEvent[n] = CreateEvent(NULL,FALSE,TRUE,NULL);
		for(auto pCurrCategory=vpCategories.begin(); pCurrCategory!=vpCategories.end(); ++pCurrCategory) {
			for(auto pCurrSequence=(*pCurrCategory)->m_vpSequences.begin(); pCurrSequence!=(*pCurrCategory)->m_vpSequences.end(); ++pCurrSequence) {
				DWORD ret = WaitForMultipleObjects(g_nMaxThreads,g_hThreadEvent,FALSE,INFINITE);
				std::cout << "\tProcessing sequence " << nSeqProcessed << "/" << nSeqTotal << "... (" << (*pCurrCategory)->m_sName << ":" << (*pCurrSequence)->m_sName << ")" << std::endl;
				nSeqProcessed++;
				g_apThreadDataStruct[ret][0] = (*pCurrCategory);
				g_apThreadDataStruct[ret][1] = (*pCurrSequence);
				g_hThreads[ret] = CreateThread(NULL,NULL,AnalyzeSequenceEntryPoint,(LPVOID)ret,0,NULL);
			}
		}
		WaitForMultipleObjects(g_nMaxThreads,g_hThreads,TRUE,INFINITE);
		for(size_t n=0; n<g_nMaxThreads; ++n) {
			CloseHandle(g_hThreadEvent[n]);
			CloseHandle(g_hThreads[n]);
		}
#endif //USE_WINDOWS_API
	}
	// let memory 'leak' here, exits faster once job is done...
	//for(auto pCurrCategory=vpCategories.begin(); pCurrCategory!=vpCategories.end(); ++pCurrCategory)
	//	delete *pCurrCategory;
	//vpCategories.clear();
}

int AnalyzeSequence(int nThreadIdx, CategoryInfo* pCurrCategory, SequenceInfo* pCurrSequence) {
	try {
		CV_DbgAssert(pCurrCategory && pCurrSequence);
		CV_DbgAssert(pCurrSequence->GetNbInputFrames()>1);
		cv::Mat oFGMask, oInputImg = pCurrSequence->GetInputFrameFromIndex(0);
#if USE_RELATIVE_LBSP_COMPARISONS
		BackgroundSubtractorLBSP oBGSubtr(LBSP_DEFAULT_REL_SIMILARITY_THRESHOLD);
#else
		BackgroundSubtractorLBSP oBGSubtr;
#endif
		oBGSubtr.initialize(oInputImg);
#if DISPLAY_ANALYSIS_DEBUG_RESULTS
		std::string sDebugDisplayName = pCurrCategory->m_sName + std::string(" -- ") + pCurrSequence->m_sName;
#if WRITE_ANALYSIS_DEBUG_RESULTS
		cv::Size oWriterInputSize = oInputImg.size();
		oWriterInputSize.height*=3;
		oWriterInputSize.width*=2;
		cv::VideoWriter oWriter(g_sResultsPath+"/"+pCurrCategory->m_sName+"/"+pCurrSequence->m_sName+".avi",CV_FOURCC('X','V','I','D'),30,oWriterInputSize,true);
#endif //WRITE_ANALYSIS_DEBUG_RESULTS
#endif //DISPLAY_ANALYSIS_DEBUG_RESULTS
		const size_t nNbInputFrames = pCurrSequence->GetNbInputFrames();
		for(size_t k=0; k<nNbInputFrames; k++) {
			if(!(k%100))
				std::cout << "\t\t" << std::setw(12) << pCurrSequence->m_sName << " @ F:" << k << "/" << nNbInputFrames << std::endl;
			oInputImg = pCurrSequence->GetInputFrameFromIndex(k);
#if DISPLAY_ANALYSIS_DEBUG_RESULTS
			cv::Mat oLastBGImg = oBGSubtr.getCurrentBGImage();
			cv::Mat oLastBGDesc = oBGSubtr.getCurrentBGDescriptors();
#endif //DISPLAY_ANALYSIS_DEBUG_RESULTS
			oBGSubtr(oInputImg, oFGMask, k<=100?1:BGSLBSP_DEFAULT_LEARNING_RATE);
#if DISPLAY_ANALYSIS_DEBUG_RESULTS
			cv::Mat oDebugDisplayFrame = GetDisplayResult(oInputImg,oLastBGImg,oLastBGDesc,oFGMask,oBGSubtr.getBGKeyPoints(),k);
			cv::imshow(sDebugDisplayName, oDebugDisplayFrame);
#if WRITE_ANALYSIS_DEBUG_RESULTS
			oWriter.write(oDebugDisplayFrame);
#endif //WRITE_ANALYSIS_DEBUG_RESULTS
			cv::waitKey(1);
#endif //DISPLAY_ANALYSIS_DEBUG_RESULTS
#if WRITE_ANALYSIS_RESULTS
			WriteResult(g_sResultsPath,pCurrCategory->m_sName,pCurrSequence->m_sName,g_sResultPrefix,k+g_nResultIdxOffset,g_sResultSuffix,oFGMask,g_vnResultsComprParams);
#endif //WRITE_ANALYSIS_RESULTS
		}
	}
	catch(cv::Exception& e) {std::cout << e.what() << std::endl;}
	catch(std::runtime_error& e) {std::cout << e.what() << std::endl;}
	catch(...) {std::cout << "Caught unknown exception." << std::endl;}
#ifndef USE_WINDOWS_API
	g_nActiveThreads--;
#else //USE_WINDOWS_API
	SetEvent(g_hThreadEvent[nThreadIdx]);
#endif //USE_WINDOWS_API
	return 0;
}

cv::Mat GetDisplayResult(const cv::Mat& oInputImg, const cv::Mat& oBGImg, const cv::Mat& oBGDesc, const cv::Mat& oFGMask, std::vector<cv::KeyPoint> voKeyPoints, size_t nFrame) {
	// note: this function is definitely NOT efficient in any way; it is only intended for debug purposes.
	cv::Mat oInputImgBYTE3, oBGImgBYTE3, oBGDescBYTE, oBGDescBYTE3, oFGMaskBYTE3;
	cv::Mat oInputDesc, oInputDescBYTE, oInputDescBYTE3;
	cv::Mat oDescDiff, oDescDiffBYTE, oDescDiffBYTE3;
	LBSP oExtractor;
	oExtractor.setReference(oBGImg);
	oExtractor.compute2(oInputImg,voKeyPoints,oInputDesc);
	LBSP::calcDescImgDiff(oInputDesc,oBGDesc,oDescDiff);
	oInputDesc.convertTo(oInputDescBYTE,CV_8U);
	oBGDesc.convertTo(oBGDescBYTE,CV_8U);
	oDescDiff.convertTo(oDescDiffBYTE,CV_8U);
	cv::cvtColor(oFGMask,oFGMaskBYTE3,CV_GRAY2RGB);
	if(oInputImg.channels()!=3) {
		cv::cvtColor(oInputImg,oInputImgBYTE3,CV_GRAY2RGB);
		cv::cvtColor(oBGImg,oBGImgBYTE3,CV_GRAY2RGB);
		cv::cvtColor(oInputDescBYTE,oInputDescBYTE3,CV_GRAY2RGB);
		cv::cvtColor(oBGDescBYTE,oBGDescBYTE3,CV_GRAY2RGB);
		cv::cvtColor(oDescDiffBYTE,oDescDiffBYTE3,CV_GRAY2RGB);
	}
	else {
		oInputImgBYTE3 = oInputImg;
		oBGImgBYTE3 = oBGImg;
		oInputDescBYTE3 = oInputDescBYTE;
		oBGDescBYTE3 = oBGDescBYTE;
		oDescDiffBYTE3 = oDescDiffBYTE;
	}
	cv::Mat display1H,display2H,display3H;
	std::stringstream sstr;
	sstr << "Input Img #" << nFrame;
	WriteOnImage(oInputImgBYTE3,sstr.str());
	WriteOnImage(oBGImgBYTE3,"BGModel Img");
	WriteOnImage(oInputDescBYTE3,"Input Desc");
	WriteOnImage(oBGDescBYTE3,"BGModel Desc");
	WriteOnImage(oFGMaskBYTE3,"Detection Result");
	WriteOnImage(oDescDiffBYTE3,"BGModel-Input Desc Diff");
	cv::hconcat(oInputImgBYTE3,oBGImgBYTE3,display1H);
	cv::hconcat(oInputDescBYTE3,oBGDescBYTE3,display2H);
	cv::hconcat(oFGMaskBYTE3,oDescDiffBYTE3,display3H);
	cv::Mat display;
	cv::vconcat(display1H,display2H,display);
	cv::vconcat(display,display3H,display);
	return display;
}
