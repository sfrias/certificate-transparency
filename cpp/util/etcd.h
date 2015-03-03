#ifndef CERT_TRANS_UTIL_ETCD_H_
#define CERT_TRANS_UTIL_ETCD_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/url_fetcher.h"
#include "util/libevent_wrapper.h"
#include "util/status.h"
#include "util/task.h"

class JsonObject;

namespace cert_trans {


class EtcdClient {
 public:
  struct Node {
    static const Node& InvalidNode();

    Node() : Node(InvalidNode()) {
    }

    Node(int64_t created_index, int64_t modified_index, const std::string& key,
         const std::string& value);

    bool HasExpiry() const;

    std::string ToString() const;

    int64_t created_index_;
    int64_t modified_index_;
    std::string key_;
    std::string value_;
    std::chrono::system_clock::time_point expires_;
    bool deleted_;
  };

  struct WatchUpdate {
    WatchUpdate();
    WatchUpdate(const Node& node, const bool exists);

    const Node node_;
    const bool exists_;
  };

  typedef std::function<void(util::Status status, const EtcdClient::Node& node,
                             int64_t etcd_index)> GetCallback;

  struct Response {
    Response() : etcd_index(-1) {
    }

    int64_t etcd_index;
  };

  struct GenericResponse : public Response {
    std::shared_ptr<JsonObject> json_body;
  };

  typedef std::function<void(util::Status status,
                             const std::vector<EtcdClient::Node>& values,
                             int64_t etcd_index)> GetAllCallback;
  typedef std::function<void(util::Status status, const std::string& key,
                             int64_t index)> CreateInQueueCallback;
  typedef std::function<void(util::Status status, int64_t new_index)>
      UpdateCallback;
  typedef std::function<void(util::Status status, int64_t new_index)>
      ForceSetCallback;
  typedef std::function<void(const std::vector<WatchUpdate>& updates)>
      WatchCallback;

  // TODO(pphaneuf): This should take a set of servers, not just one.
  EtcdClient(const std::shared_ptr<libevent::Base>& event_base,
             UrlFetcher* fetcher, const std::string& host, uint16_t port);

  virtual ~EtcdClient();

  void Get(const std::string& key, const GetCallback& cb);

  void GetAll(const std::string& dir, const GetAllCallback& cb);

  void Create(const std::string& key, const std::string& value, Response* resp,
              util::Task* task);

  void CreateWithTTL(const std::string& key, const std::string& value,
                     const std::chrono::seconds& ttl, Response* resp,
                     util::Task* task);

  void CreateInQueue(const std::string& dir, const std::string& value,
                     const CreateInQueueCallback& cb);

  void Update(const std::string& key, const std::string& value,
              const int64_t previous_index, const UpdateCallback& cb);

  void UpdateWithTTL(const std::string& key, const std::string& value,
                     const std::chrono::seconds& ttl,
                     const int64_t previous_index, const UpdateCallback& cb);

  void ForceSet(const std::string& key, const std::string& value,
                const ForceSetCallback& cb);

  void ForceSetWithTTL(const std::string& key, const std::string& value,
                       const std::chrono::seconds& ttl,
                       const ForceSetCallback& cb);

  void Delete(const std::string& key, const int64_t current_index,
              util::Task* task);

  // The "cb" will be called on the "task" executor. Also, only one
  // will be sent to the executor at a time (for a given call to this
  // method, not for all of them), to make sure they are received in
  // order.
  virtual void Watch(const std::string& key, const WatchCallback& cb,
                     util::Task* task);

 protected:
  typedef std::function<void(util::Status status,
                             const std::shared_ptr<JsonObject>&,
                             int64_t etcd_index)> GenericCallback;

  // Testing only
  EtcdClient(const std::shared_ptr<libevent::Base>& event_base);

  virtual void Generic(const std::string& key,
                       const std::map<std::string, std::string>& params,
                       UrlFetcher::Verb verb, GenericResponse* resp,
                       util::Task* task);

 private:
  typedef std::pair<std::string, uint16_t> HostPortPair;

  struct Request;
  struct WatchState;

  HostPortPair GetEndpoint() const;
  HostPortPair UpdateEndpoint(const std::string& host, uint16_t port);
  void FetchDone(Request* etcd_req, util::Task* task);

  void WatchInitialGetDone(WatchState* state, util::Status status,
                           const Node& node, int64_t etcd_index);
  void WatchInitialGetAllDone(WatchState* state, util::Status status,
                              const std::vector<Node>& nodes,
                              int64_t etcd_index);
  void SendWatchUpdates(WatchState* state,
                        const std::vector<WatchUpdate>& updates);
  void StartWatchRequest(WatchState* state);
  void WatchRequestDone(WatchState* state, GenericResponse* gen_resp,
                        util::Task* child_task);

  const std::shared_ptr<libevent::Base> event_base_;
  UrlFetcher* const fetcher_;

  mutable std::mutex lock_;
  HostPortPair endpoint_;

  DISALLOW_COPY_AND_ASSIGN(EtcdClient);
};


}  // namespace cert_trans

#endif  // CERT_TRANS_UTIL_ETCD_H_
