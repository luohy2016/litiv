
// This file is part of the LITIV framework; visit the original repository at
// https://github.com/plstcharles/litiv for more information.
//
// Copyright 2015 Pierre-Luc St-Charles; pierre-luc.st-charles<at>polymtl.ca
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#define DEFAULT_BSDS500_EDGE_EVAL_THRESHOLD_BINS 99

#include "litiv/datasets/utils.hpp"

namespace litiv {

    //! basic metrics counter used to evaluate binary classifiers
    struct ClassifMetricsBase {
        //! default constructor sets all counters to zero
        ClassifMetricsBase();
        ClassifMetricsBase operator+(const ClassifMetricsBase& m) const;
        ClassifMetricsBase& operator+=(const ClassifMetricsBase& m);
        inline uint64_t total() const {return nTP+nTN+nFP+nFN;}
        uint64_t nTP;
        uint64_t nTN;
        uint64_t nFP;
        uint64_t nFN;
        uint64_t nSE; // 'shadow error' counter used in segm (extra that will not affect others)
        enum eCountersList { // used for packed array indexing
            eCounter_TP,
            eCounter_TN,
            eCounter_FP,
            eCounter_FN,
            eCounter_SE,
            eCount,
        };
    };

    //! high-level metrics counter used to evaluate binary classifiers (relies on ClassifMetricsBase internally)
    struct ClassifMetrics {
        //! default contructor requires a base metrics counters, as otherwise, we may obtain NaN's
        ClassifMetrics(const ClassifMetricsBase& m);
        ClassifMetrics operator+(const ClassifMetrics& m) const;
        ClassifMetrics& operator+=(const ClassifMetrics& m);
        double dRecall;
        double dSpecificity;
        double dFPR;
        double dFNR;
        double dPBC;
        double dPrecision;
        double dFMeasure;
        double dMCC;
        size_t nWeight; // used to compute iterative averages in overloads only
        static double CalcFMeasure(double dRecall, double dPrecision);
        static double CalcFMeasure(const ClassifMetricsBase& m);
        static double CalcRecall(uint64_t nTP, uint64_t nTPFN);
        static double CalcRecall(const ClassifMetricsBase& m);
        static double CalcPrecision(uint64_t nTP, uint64_t nTPFP);
        static double CalcPrecision(const ClassifMetricsBase& m);
        static double CalcSpecificity(const ClassifMetricsBase& m);
        static double CalcFalsePositiveRate(const ClassifMetricsBase& m);
        static double CalcFalseNegativeRate(const ClassifMetricsBase& m);
        static double CalcPercentBadClassifs(const ClassifMetricsBase& m);
        static double CalcMatthewsCorrCoeff(const ClassifMetricsBase& m);
    };

    template<eDatasetTypeList eDatasetType>
    struct IDatasetEvaluator_ : public IDataset {
        //! writes an overall evaluation report listing packet counts, seconds elapsed and algo speed
        virtual void writeEvalReport() const override {
            std::cout << "Writing evaluation report for dataset '" << getName() << "'..." << std::endl;
            if(getBatches().empty()) {
                std::cout << "\tNo report to write for dataset '" << getName() << "', skipping." << std::endl;
                return;
            }
            for(const auto& pGroupIter : getBatches())
                pGroupIter->writeEvalReport();
            std::ofstream oMetricsOutput(getOutputPath()+"/overall.txt");
            if(oMetricsOutput.is_open()) {
                oMetricsOutput << std::fixed;
                oMetricsOutput << "Default evaluation report for dataset '" << getName() << "' :\n\n";
                oMetricsOutput << "            |   Packets  |   Seconds  |     Hz     \n";
                oMetricsOutput << "------------|------------|------------|------------\n";
                size_t nOverallPacketCount = 0;
                double dOverallTimeElapsed = 0.0;
                for(const auto& pGroupIter : getBatches()) {
                    oMetricsOutput << pGroupIter->writeInlineEvalReport(0);
                    nOverallPacketCount += pGroupIter->getTotPackets();
                    dOverallTimeElapsed += pGroupIter->getProcessTime();
                }
                oMetricsOutput << "------------|------------|------------|------------\n";
                oMetricsOutput << "     overall|" <<
                                  std::setw(12) << nOverallPacketCount << "|" <<
                                  std::setw(12) << dOverallTimeElapsed << "|" <<
                                  std::setw(12) << nOverallPacketCount/dOverallTimeElapsed << "\n";
                oMetricsOutput << "\nSHA1:" << LITIV_VERSION_SHA1 << "\n[" << CxxUtils::getTimeStamp() << "]" << std::endl;
            }
        }
    };

    template<>
    struct IDatasetEvaluator_<eDatasetType_VideoSegm> : public IDataset {
        //! writes an overall evaluation report listing high-level binary classification metrics
        virtual void writeEvalReport() const override;
        //! accumulates overall basic metrics from all batch(es)
        virtual ClassifMetricsBase getMetricsBase() const;
        //! accumulates overall high-level metrics from all batch(es)
        virtual ClassifMetrics getMetrics(bool bAverage) const;
    };

    template<eDatasetTypeList eDatasetType, eDatasetList eDataset>
    struct DatasetEvaluator_ : public IDatasetEvaluator_<eDatasetType> {}; // no evaluation specialization by default

    template<eDatasetTypeList eDatasetType>
    struct IMetricsCalculator_ : public virtual IDataHandler {
        //! returns a one-line string listing packet counts, seconds elapsed and algo speed for current batch(es)
        virtual std::string writeInlineEvalReport(size_t nIndentSize, size_t nCellSize=12) const override {
            if(!this->getTotPackets())
                return std::string();
            std::stringstream ssStr;
            ssStr << std::fixed;
            if(this->isGroup() && !this->isBare())
                for(const auto& pBatch : this->getBatches())
                    ssStr << pBatch->writeInlineEvalReport(nIndentSize+1);
            ssStr << CxxUtils::clampString((std::string(nIndentSize,'>')+' '+this->getName()),nCellSize) << "|" <<
                     std::setw(nCellSize) << this->getTotPackets() << "|" <<
                     std::setw(nCellSize) << this->getProcessTime() << "|" <<
                     std::setw(nCellSize) << this->getTotPackets()/this->getProcessTime() << "\n";
            return ssStr.str();
        }
        //! writes an evaluation report listing packet counts, seconds elapsed and algo speed for current batch(es)
        virtual void writeEvalReport() const override {
            if(!this->getTotPackets()) {
                std::cout << "No report to write for '" << this->getName() << "', skipping..." << std::endl;
                return;
            }
            if(this->isGroup() && !this->isBare()) {
                for(const auto& pBatch : this->getBatches())
                    pBatch->writeEvalReport();
            }
            std::ofstream oMetricsOutput(this->getOutputPath()+"/../"+this->getName()+".txt");
            if(oMetricsOutput.is_open()) {
                oMetricsOutput << std::fixed;
                oMetricsOutput << "Default evaluation report for '" << this->getName() << "' :\n\n";
                oMetricsOutput << "            |   Packets  |   Seconds  |     Hz     \n";
                oMetricsOutput << "------------|------------|------------|------------\n";
                oMetricsOutput << this->writeInlineEvalReport(0);
                oMetricsOutput << "\nSHA1:" << LITIV_VERSION_SHA1 << "\n[" << CxxUtils::getTimeStamp() << "]" << std::endl;
            }
        }
    };

    template<>
    struct IMetricsCalculator_<eDatasetType_VideoSegm> : public virtual IDataHandler {
        //! accumulates basic metrics from current batch(es) --- provides group-impl only
        virtual ClassifMetricsBase getMetricsBase() const;
        //! accumulates high-level metrics from current batch(es)
        virtual ClassifMetrics getMetrics(bool bAverage) const;
        //! returns a one-line string listing high-level metrics for current batch(es)
        virtual std::string writeInlineEvalReport(size_t nIndentSize, size_t nCellSize=12) const override;
        //! writes an evaluation report listing high-level metrics for current batch(es)
        virtual void writeEvalReport() const override;
    };

    template<eDatasetTypeList eDatasetType, eGroupPolicy ePolicy>
    struct IDataEvaluator_ :
            public IMetricsCalculator_<eDatasetType>,
            public IDataConsumer_<eDatasetType,ePolicy> {}; // no evaluation specialization by default

    template<>
    struct IDataEvaluator_<eDatasetType_VideoSegm,eNotGroup> :
            public IMetricsCalculator_<eDatasetType_VideoSegm>,
            public IDataConsumer_<eDatasetType_VideoSegm,eNotGroup> {
        //! overrides 'getMetricsBase' from IMetricsCalculator_ for non-group-impl (as always required)
        virtual ClassifMetricsBase getMetricsBase() const override;
        //! overrides 'pushSegmMask' from IDataConsumer_ to simultaneously evaluate the pushed results
        virtual void pushSegmMask(const cv::Mat& oSegm,size_t nIdx) override;
        //! provides a visual feedback on result quality based on evaluation guidelines
        virtual cv::Mat getColoredSegmMask(const cv::Mat& oSegm, size_t nIdx);

#if 0//HAVE_GLSL
    protected:
        struct GLVideoSegmDataEvaluator :
                public IAsyncDataEvaluator<eDatasetType_VideoSegm,ParallelUtils::eGLSL>,
                public GLImageProcEvaluatorAlgo {
            GLVideoSegmDataEvaluator(const std::shared_ptr<GLImageProcAlgo>& pParent, size_t nTotFrameCount);
            virtual std::string getComputeShaderSource(size_t nStage) const override;
            virtual ClassifMetricsBase getCumulativeMetrics();
        };
        using GLVideoSegmDataEvaluatorPtr = std::unique_ptr<GLVideoSegmDataEvaluator>;
        GLVideoSegmDataEvaluatorPtr m_pGLEvaluator;

    public:
        //! inits a glsl algo evaluator for the datset, and returns the expected gl context window size for display (if needed)
        virtual cv::Size init_GLSL(const std::shared_ptr<GLImageProcAlgo>& pAlgo) override {
            m_pGLEvaluator = std::make_unique<GLVideoSegmDataEvaluator>(pAlgo,getTotPackets());
            auto pProducer = std::dynamic_pointer_cast<IDataProducer_<eDatasetType_VideoSegm,eNotGroup>>(shared_from_this());
            CV_Assert(pProducer);
            m_pGLEvaluator->initialize_gl(pProducer->getGTFrame(0),pProducer->getROI());
            cv::Size oExpectedDisplaySize = pProducer->getFrameSize();
            oExpectedDisplaySize.width *= m_pGLEvaluator->m_nSxSDisplayCount;
            return oExpectedDisplaySize;
        }
        //! feeds the 'nIdx+1' frame to the gl algo (passed via initwhile fetching the 'nIdx-1' frame for evaluation
        virtual void apply_GLSL(size_t nIdx) {
            CV_Assert(m_pGLEvaluator);
            m_pGLEvaluator->apply_gl();
        }
        //virtual std::shared_ptr<GLEvaluator> CreateGLEvaluator(const std::shared_ptr<GLImageProcAlgo>& pParent, size_t nTotFrameCount) const {
        //    return std::shared_ptr<GLEvaluator>(new GLEvaluator(pParent,nTotFrameCount));
        //}
        //virtual void FetchGLEvaluation(std::shared_ptr<IGLEvaluator_<eDatasetType_VideoSegm>> pGLEvaluator);

#endif //HAVE_GLSL

        static const uchar s_nSegmPositive;
        static const uchar s_nSegmNegative;
        static const uchar s_nSegmOutOfScope;
        static const uchar s_nSegmUnknown;
        static const uchar s_nSegmShadow;

    protected:
        ClassifMetricsBase m_oMetricsBase;
    };

    template<eDatasetTypeList eDatasetType, eDatasetList eDataset, eGroupPolicy ePolicy>
    struct DataEvaluator_ : public IDataEvaluator_<eDatasetType,ePolicy> {};

#if 0

    namespace Image {

        namespace Segm {

            struct BSDS500BoundaryEvaluator : public IEvaluator {
                BSDS500BoundaryEvaluator(size_t nThresholdBins=DEFAULT_BSDS500_EDGE_EVAL_THRESHOLD_BINS);
                virtual cv::Mat getColoredSegmMask(const cv::Mat& oSegmMask, const cv::Mat& oGTSegmMask, const cv::Mat& /*oUnused*/) const;
                virtual void accumulateMetrics(const cv::Mat& oSegmMask, const cv::Mat& oGTSegmMask, const cv::Mat& /*oUnused*/);
                static const double s_dMaxImageDiagRatioDist;
                struct BSDS500ClassifMetricsBase { // basic eval metrics for a single image
                    BSDS500ClassifMetricsBase(size_t nThresholdsBins); // always skips zero threshold
                    std::vector<uint64_t> vnIndivTP; // one count per threshold
                    std::vector<uint64_t> vnIndivTPFN; // one count per threshold
                    std::vector<uint64_t> vnTotalTP; // one count per threshold
                    std::vector<uint64_t> vnTotalTPFP; // one count per threshold
                    const std::vector<uchar> vnThresholds; // list of thresholds
                };
                void setThresholdBins(size_t nThresholdBins);
                size_t getThresholdBins() const;
            protected:
                size_t m_nThresholdBins;
                std::vector<BSDS500ClassifMetricsBase> m_voClassifMetricsBase;
                struct BSDS500Score { // edge detection score for a single threshold
                    double dThreshold;
                    double dRecall;
                    double dPrecision;
                    double dFMeasure;
                };
                struct BSDS500Metrics { // high-level metrics for an entire image set
                    std::vector<BSDS500Score> voBestImageScores; // one score per image (best threshold)
                    std::vector<BSDS500Score> voThresholdScores; // one score per threshold (cumul images)
                    BSDS500Score oBestScore; // best score for all thresholds
                    double dMaxRecall;
                    double dMaxPrecision;
                    double dMaxFMeasure;
                    double dAreaPR;
                };
                static void CalcMetrics(const WorkBatch& oBatch, BSDS500Metrics& oRes);
                static void writeEvalReport(const DatasetInfoBase& oInfo, const std::vector<std::shared_ptr<WorkGroup>>& vpGroups);
                static void writeEvalReport(const WorkBatch& oBatch, BSDS500Metrics& oRes);
                static BSDS500Score FindMaxFMeasure(const std::vector<uchar>& vnThresholds, const std::vector<double>& vdRecall, const std::vector<double>& vdPrecision);
                static BSDS500Score FindMaxFMeasure(const std::vector<BSDS500Score>& voScores);
                friend struct DatasetInfo;
            };

        } //namespace Segm

    } //namespace Image

#endif

} //namespace litiv