#pragma once

#include <cstdint>
#include <memory>

#include <rendering/ResourceDomain.hpp>
#include <rendering/ResourceLedger.hpp>

namespace playground::rendering {

using SubmissionId = std::uint64_t;

// A lease records use, not ownership of the native allocation. Keeping this
// separate from device-owning handles avoids device -> pending work -> device
// ownership cycles. Recordings and pending submissions retain these leases.
struct ResourceUse {
  ResourceDomainId domain;
  SubmissionId lastSubmission{};
  ResourceLedger::Token allocation;
};

using ResourceLease = std::shared_ptr<ResourceUse>;

} // namespace playground::rendering
