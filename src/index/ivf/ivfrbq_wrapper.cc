// Copyright (C) 2019-2024 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

#include "index/ivf/ivfrbq_wrapper.h"
#include "faiss/IndexFlat.h"
#include "faiss/IndexIVFRaBitQ.h"
#include "faiss/IndexRefine.h"
#include "faiss/IndexPreTransform.h"
#include "index/refine/refine_utils.h"

#include <memory>

#include "faiss/IndexCosine.h"
#include "faiss/cppcontrib/knowhere/impl/CountSizeIOWriter.h"
#include "faiss/index_io.h"

namespace knowhere {

IndexIVFRaBitQWrapper::IndexIVFRaBitQWrapper(std::unique_ptr<faiss::Index>&& index_in)
    : faiss::Index(index_in->d, index_in->metric_type), index(std::move(index_in)) {}

expected<std::unique_ptr<IndexIVFRaBitQWrapper>>
IndexIVFRaBitQWrapper::create(const faiss::idx_t d, const size_t nlist, const IvfRaBitQConfig& ivf_rabitq_cfg,
                             const DataFormatEnum raw_data_format, const faiss::MetricType metric) {
    // Create quantizer
    auto quantizer = std::make_unique<faiss::IndexFlat>(d, metric);
    // Create IVF RaBitQ index
    auto idx_rr = std::make_unique<faiss::IndexIVFRaBitQ>(quantizer.release(), d, nlist);
    idx_rr->own_fields = true;

    // Create the base index with refinement if specified
    auto final_index = pick_refine_index(raw_data_format, ivf_rabitq_cfg.refine_type, std::move(idx_rr), d, metric);
    if (!final_index.has_value()) {
        return expected<std::unique_ptr<IndexIVFRaBitQWrapper>>::Err(
            Status::invalid_args,
            "Failed to create refine index"
        );
    }

    return std::make_unique<IndexIVFRaBitQWrapper>(std::move(final_index.value()));
}

std::unique_ptr<IndexIVFRaBitQWrapper>
IndexIVFRaBitQWrapper::from_deserialized(std::unique_ptr<faiss::Index>&& index_in) {
    return std::make_unique<IndexIVFRaBitQWrapper>(std::move(index_in));
}

faiss::IndexIVFRaBitQ*
IndexIVFRaBitQWrapper::get_ivfrabitq_index() {
    if (auto* pt = dynamic_cast<faiss::IndexPreTransform*>(index.get())) {
        return dynamic_cast<faiss::IndexIVFRaBitQ*>(pt->index);
    }
    return dynamic_cast<faiss::IndexIVFRaBitQ*>(index.get());
}

const faiss::IndexIVFRaBitQ*
IndexIVFRaBitQWrapper::get_ivfrabitq_index() const {
    if (auto* pt = dynamic_cast<const faiss::IndexPreTransform*>(index.get())) {
        return dynamic_cast<const faiss::IndexIVFRaBitQ*>(pt->index);
    }
    return dynamic_cast<const faiss::IndexIVFRaBitQ*>(index.get());
}

faiss::IndexRefine*
IndexIVFRaBitQWrapper::get_refine_index() {
    if (auto* pt = dynamic_cast<faiss::IndexPreTransform*>(index.get())) {
        return dynamic_cast<faiss::IndexRefine*>(pt->index);
    }
    return dynamic_cast<faiss::IndexRefine*>(index.get());
}

const faiss::IndexRefine*
IndexIVFRaBitQWrapper::get_refine_index() const {
    if (auto* pt = dynamic_cast<const faiss::IndexPreTransform*>(index.get())) {
        return dynamic_cast<const faiss::IndexRefine*>(pt->index);
    }
    return dynamic_cast<const faiss::IndexRefine*>(index.get());
}

size_t
IndexIVFRaBitQWrapper::size() const {
    return index->ntotal;
}

std::unique_ptr<faiss::IVFIteratorWorkspace>
IndexIVFRaBitQWrapper::getIteratorWorkspace(const float* query_data, const faiss::IVFSearchParameters* ivfsearchParams) const {
    auto* ivf = get_ivfrabitq_index();
    if (!ivf) return nullptr;
    return std::make_unique<faiss::IVFIteratorWorkspace>(query_data, index->d, ivfsearchParams);
}

void
IndexIVFRaBitQWrapper::getIteratorNextBatch(faiss::IVFIteratorWorkspace* workspace, size_t current_backup_count) const {
    auto* ivf = get_ivfrabitq_index();
    if (!ivf) return;
    ivf->getIteratorNextBatch(workspace, current_backup_count);
}

void
IndexIVFRaBitQWrapper::train(faiss::idx_t n, const float* x) {
    index->train(n, x);
}

void
IndexIVFRaBitQWrapper::add(faiss::idx_t n, const float* x) {
    index->add(n, x);
}

void
IndexIVFRaBitQWrapper::search(faiss::idx_t n, const float* x, faiss::idx_t k,
                           float* distances, faiss::idx_t* labels,
                           const faiss::SearchParameters* params) const {
    index->search(n, x, k, distances, labels, params);
}

void
IndexIVFRaBitQWrapper::range_search(faiss::idx_t n, const float* x, float radius,
                                 faiss::RangeSearchResult* result,
                                 const faiss::SearchParameters* params) const {
    index->range_search(n, x, radius, result, params);
}

void
IndexIVFRaBitQWrapper::reset() {
    index->reset();
}

void
IndexIVFRaBitQWrapper::merge_from(faiss::Index& otherIndex, faiss::idx_t add_id) {
    index->merge_from(otherIndex, add_id);
}

faiss::DistanceComputer*
IndexIVFRaBitQWrapper::get_distance_computer() const {
    return index->get_distance_computer();
}

}  // namespace knowhere
