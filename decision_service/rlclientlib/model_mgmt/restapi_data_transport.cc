#include "restapi_data_transport.h"
#include <cpprest/http_client.h>
#include <cpprest/asyncrt_utils.h>
#include <cpprest/rawptrstream.h>
#include "api_status.h"
#include "factory_resolver.h"
#include "utility/http_helper.h"

using namespace web; // Common features like URIs.
using namespace web::http; // Common HTTP functionality
using namespace std::chrono;

namespace u = reinforcement_learning::utility; 

namespace reinforcement_learning { namespace model_management {

  restapi_data_tranport::restapi_data_tranport(const std::string& url)
    : _url(url), _httpcli(::utility::conversions::to_string_t(_url), u::get_http_config()), _datasz{0} { }

  /*
   * Example successful response
   *
   * Received response status code:200
   * Accept-Ranges = bytes
   * Content-Length = 7666
   * Content-MD5 = VuJg8VgcBQwevGhJR2Yehw==
   * Content-Type = application/octet-stream
   * Date = Mon, 28 May 2018 14:41:02 GMT
   * ETag = "0x8D5C03A2AEC2189"
   * Last-Modified = Tue, 22 May 2018 23:17:20 GMT
   * Server = Windows-Azure-Blob/1.0 Microsoft-HTTPAPI/2.0
   * x-ms-blob-type = BlockBlob
   * x-ms-lease-state = available
   * x-ms-lease-status = unlocked
   * x-ms-request-id = 241f3513-801e-0041-0991-f6893e000000
   * x-ms-server-encrypted = true
   * x-ms-version = 2017-04-17
   */

  int restapi_data_tranport::get_data_info(::utility::datetime& last_modified, ::utility::size64_t& sz, api_status* status) {

    // Build request URI and start the request.
    auto request_task = _httpcli.request(methods::HEAD)
      // Handle response headers arriving.
      .then([&](http_response response) {
      if ( response.status_code() != 200 ) 
        RETURN_ERROR_ARG(status, http_bad_status_code, _url);

      const auto iter = response.headers().find(U("Last-Modified"));
      if ( iter == response.headers().end() )
        RETURN_ERROR_ARG(status, last_modified_not_found, _url);

      last_modified = ::utility::datetime::from_string(iter->second);
      if( last_modified.to_interval() == 0)
        RETURN_ERROR_ARG(status, last_modified_invalid, _url);

      sz = response.headers().content_length();

      return error_code::success;
    });

    // Wait for all the outstanding I/O to complete and handle any exceptions
    try {
      return request_task.get();
    }
    catch ( const std::exception &e ) {
      RETURN_ERROR_LS(status,exception_during_http_req) << e.what() << "\n URL: " << _url;
    }
  }

  int restapi_data_tranport::get_data(model_data& ret, api_status* status) {

    ::utility::datetime curr_last_modified;
    ::utility::size64_t curr_datasz;
    const auto err = get_data_info(curr_last_modified, curr_datasz, status);
    RETURN_IF_FAIL(err);

    if ( curr_last_modified == _last_modified && curr_datasz == _datasz )
      return error_code::success;

    // Build request URI and start the request.
    auto request_task = _httpcli.request(methods::GET)
      // Handle response headers arriving.
      .then([&](pplx::task<http_response> respTask) {
      auto response = respTask.get();
      if ( response.status_code() != 200 )
        RETURN_ERROR_ARG(status, http_bad_status_code, "Found: ", response.status_code(), _url);

      const auto iter = response.headers().find(U("Last-Modified"));
      if ( iter == response.headers().end() )
        RETURN_ERROR_ARG(status, last_modified_not_found, _url);

      curr_last_modified = ::utility::datetime::from_string(iter->second);
      if ( curr_last_modified.to_interval() == 0 )
        RETURN_ERROR_ARG(status, last_modified_invalid, "Found: ", 
          ::utility::conversions::to_utf8string(curr_last_modified.to_string()), _url);

      curr_datasz = response.headers().content_length();
      if ( curr_datasz > 0 ) {
        const auto buff = ret.alloc(curr_datasz);
        const Concurrency::streams::rawptr_buffer<char> rb(buff, curr_datasz, std::ios::out);

        // Write response body into the file.
        const auto readval = response.body().read_to_end(rb).get();  // need to use task.get to throw exceptions properly

        ret.data_sz(readval);
        ret.increment_refresh_count();
        _datasz = readval;
      }
      else {
        ret.data_sz(0);
      }
      
      _last_modified = curr_last_modified;
      return error_code::success;
    });

    // Wait for all the outstanding I/O to complete and handle any exceptions
    try {
      request_task.wait();
    }
    catch ( const std::exception &e ) {
      ret.free();
      RETURN_ERROR_LS(status, exception_during_http_req) <<  e.what();
    }
    catch ( ... ) {
      ret.free();
      RETURN_ERROR_LS(status, exception_during_http_req) << error_code::unkown_s;
    }

    return error_code::success;
  }

  int restapi_data_tranport::check(api_status* status) {
    ::utility::datetime last_modified;
    ::utility::size64_t datasz;
    return get_data_info(last_modified, datasz, status);
  }
}}