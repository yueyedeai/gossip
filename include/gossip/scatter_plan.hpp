#pragma once

#include <vector>
#include <iostream>

#include "transfer_plan.hpp"

namespace gossip {

template<
    bool throw_exceptions=true>
class scatter_plan_t : public transfer_plan_t<throw_exceptions> {

    gpu_id_t source;

public:
    scatter_plan_t(const gpu_id_t source_,
                   const gpu_id_t num_gpus_,
                   const std::vector<std::vector<gpu_id_t>>& transfer_sequences_ = {})
                   : transfer_plan_t<throw_exceptions>(num_gpus_, transfer_sequences_),
                     source(source_)
    {
        initialize();
    }

    scatter_plan_t(const gpu_id_t source_,
                   const gpu_id_t num_gpus_,
                   const std::vector<std::vector<gpu_id_t>>& transfer_sequences_,
                   const size_t num_chunks_,
                   const std::vector<size_t>& transfer_sizes_)
                   : transfer_plan_t<throw_exceptions>(num_gpus_, transfer_sequences_,
                                                       num_chunks_, transfer_sizes_),
                     source(source_)
    {
        initialize();
    }

private:
    void initialize() {
        if(this->num_gpus >= 2) {
            if(this->transfer_sequences.empty()) load_default_plan();
            this->num_steps = this->transfer_sequences[0].size()-1;
            this->synchronized = false;
            // trim_plan();
            this->valid = verify_plan();
        }
    }

    void load_default_plan() override {
        this->num_steps = 1;
        this->num_chunks = 1;

        this->transfer_sequences.clear();
        this->transfer_sequences.reserve(this->num_gpus);

        // plan direct transfers from source to trg gpu
        for (gpu_id_t trg = 0; trg < this->num_gpus; ++trg) {
            this->transfer_sequences.emplace_back(std::vector<gpu_id_t>{source,trg});
        }
    }

    void trim_plan() {
        for (auto& sequence : this->transfer_sequences) {
            // auto seq_len = sequence.size();
            auto target = gpu_id_t(-1);
            for (auto it = sequence.rbegin(); it < sequence.rend(); ++it) {
                // trim trailing -1
                if(target == gpu_id_t(-1))
                    target = *it;
                // trim trailing items == target
                else if(target != *it) {
                    sequence.resize((sequence.rend() - it) + 1);
                    break;
                }
            }
        }
    }

    bool verify_plan() const override {
        if (this->num_steps < 1)
            if (throw_exceptions)
                throw std::invalid_argument(
                    "planned sequence must be at least of length 2.");
            else return false;

        for (const auto& sequence : this->transfer_sequences)
            if (sequence.size() != this->num_steps+1)
                if (throw_exceptions)
                    throw std::invalid_argument(
                        "planned sequences must have same lengths.");
                else return false;

        for (const auto& sequence : this->transfer_sequences) {
            if (sequence.front() != source)
                if (throw_exceptions)
                    throw std::invalid_argument(
                        "all sequences must have same source.");
                else return false;
        }

        std::vector<size_t> completeness(this->num_gpus);
        if (this->num_chunks <= 1) {
            for (const auto& sequence : this->transfer_sequences) {
                completeness[sequence.back()] += 1;
            }
        }
        else {
            if (this->transfer_sequences.size() != this->transfer_sizes.size())
                if (throw_exceptions)
                    throw std::invalid_argument(
                        "number of sequences must match number of sizes.");
                else return false;
           for (size_t i = 0; i < this->transfer_sequences.size(); ++i) {
                completeness[this->transfer_sequences[i].back()]
                    += this->transfer_sizes[i];
            }           
        }
        for (gpu_id_t trg = 0; trg < this->num_gpus; ++trg) {
            if (completeness[trg] != this->num_chunks)
                if (throw_exceptions)
                    throw std::invalid_argument(
                        "transfer plan is incomplete.");
                else return false;
        }

        return true;
    }

public:
    gpu_id_t get_main_gpu() const noexcept {
        return source;
    }
};

} // namespace