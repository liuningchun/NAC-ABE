/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2017-2019, Regents of the University of California.
 *
 * This file is part of NAC-ABE.
 *
 * NAC-ABE is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * NAC-ABE is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * NAC-ABE, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of NAC-ABE authors and contributors.
 */

#include "attribute-authority.hpp"
#include "attribute-authority-token.hpp"
#include "consumer.hpp"
#include "data-owner.hpp"
#include "producer.hpp"
#include "token-issuer.hpp"
#include "test-common.hpp"
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace ndn {
namespace nacabe {
namespace tests {

namespace fs = boost::filesystem;

const uint8_t PLAIN_TEXT[1024] = {1};

NDN_LOG_INIT(Test.IntegratedTest);

class TestIntegratedFixture : public IdentityManagementTimeFixture
{
public:
  TestIntegratedFixture()
    : producerFace(io, m_keyChain, util::DummyClientFace::Options{true, true})
    , aaFace(io, m_keyChain, util::DummyClientFace::Options{true, true})
    , tokenIssuerFace(io, m_keyChain, util::DummyClientFace::Options{true, true})
    , consumerFace1(io, m_keyChain, util::DummyClientFace::Options{true, true})
    , consumerFace2(io, m_keyChain, util::DummyClientFace::Options{true, true})
    , dataOwnerFace(io, m_keyChain, util::DummyClientFace::Options{true, true})
  {
    producerFace.linkTo(aaFace);
    producerFace.linkTo(tokenIssuerFace);
    producerFace.linkTo(consumerFace1);
    producerFace.linkTo(consumerFace2);
    producerFace.linkTo(dataOwnerFace);

    security::Identity aaId = addIdentity("/aaPrefix");
    security::Key aaKey = aaId.getDefaultKey();
    aaCert = aaKey.getDefaultCertificate();

    security::Identity tokenIssuerId = addIdentity("/tokenIssuerPrefix");
    security::Key tokenIssuerKey = tokenIssuerId.getDefaultKey();
    tokenIssuerCert = tokenIssuerKey.getDefaultCertificate();

    security::Identity consumerId1 = addIdentity("/consumerPrefix1");
    security::Key consumerKey1 = m_keyChain.createKey(consumerId1, RsaKeyParams());
    consumerCert1 = consumerKey1.getDefaultCertificate();

    security::Identity consumerId2 = addIdentity("/consumerPrefix2");
    security::Key consumerKey2 = m_keyChain.createKey(consumerId2, RsaKeyParams());
    consumerCert2 = consumerKey2.getDefaultCertificate();

    security::Identity producerId = addIdentity("/producerPrefix");
    security::Key producerKey = producerId.getDefaultKey();
    producerCert = producerKey.getDefaultCertificate();

    security::Identity dataOwnerId = addIdentity("/dataOwnerPrefix");
    security::Key dataOwnerKey = dataOwnerId.getDefaultKey();
    dataOwnerCert = dataOwnerKey.getDefaultCertificate();
  }

public:
  util::DummyClientFace producerFace;
  util::DummyClientFace aaFace;
  util::DummyClientFace tokenIssuerFace;
  util::DummyClientFace consumerFace1;
  util::DummyClientFace consumerFace2;
  util::DummyClientFace dataOwnerFace;

  security::v2::Certificate aaCert;
  security::v2::Certificate tokenIssuerCert;
  security::v2::Certificate consumerCert1;
  security::v2::Certificate consumerCert2;
  security::v2::Certificate producerCert;
  security::v2::Certificate dataOwnerCert;
};

BOOST_FIXTURE_TEST_SUITE(TestIntegrated, TestIntegratedFixture)

BOOST_AUTO_TEST_CASE(IntegratedTest)
{
  // set up AA
  NDN_LOG_INFO("Create Attribute Authority. AA prefix: " << aaCert.getIdentity());
  AttributeAuthority aa = AttributeAuthority(aaCert, aaFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  // define attr list for consumer rights
  std::list<std::string> attrList = {"attr1", "attr3"};
  NDN_LOG_INFO("Add comsumer 1 "<<consumerCert1.getIdentity()<<" with attributes: attr1, attr3");
  aa.m_tokens.insert(std::pair<Name, std::list<std::string>>(consumerCert1.getIdentity(),
                                                                      attrList));
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 1);


  std::list<std::string> attrList1 = {"attr1"};
  NDN_LOG_INFO("Add comsumer 2 "<<consumerCert2.getIdentity()<<" with attributes: attr1");
  aa.m_tokens.insert(std::pair<Name, std::list<std::string>>(consumerCert2.getIdentity(),
                                                                      attrList1));
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 2);

  // set up consumer
  NDN_LOG_INFO("Create Consumer 1. Consumer 1 prefix:"<<consumerCert1.getIdentity());
  Consumer consumer1 = Consumer(consumerCert1, consumerFace1, m_keyChain, aaCert.getIdentity());
  aa.m_trustConfig.m_trustAnchors.push_back(consumerCert1);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer1.m_pubParamsCache.m_pub != "");

  // set up consumer
  NDN_LOG_INFO("Create Consumer 2. Consumer 2 prefix:"<<consumerCert2.getIdentity());
  Consumer consumer2 = Consumer(consumerCert2, consumerFace2, m_keyChain, aaCert.getIdentity());
  aa.m_trustConfig.m_trustAnchors.push_back(consumerCert2);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer2.m_pubParamsCache.m_pub != "");

  // set up producer
  NDN_LOG_INFO("Create Producer. Producer prefix:"<<producerCert.getIdentity());
  Producer producer = Producer(producerCert, producerFace, m_keyChain, aaCert.getIdentity());
  advanceClocks(time::milliseconds(20), 60);

  BOOST_CHECK(producer.m_pubParamsCache.m_pub != "");
  BOOST_CHECK_EQUAL(producer.m_interestFilterIds.size(), 1);

  // set up data owner
  NDN_LOG_INFO("Create Data Owner. Data Owner prefix:"<<dataOwnerCert.getIdentity());
  DataOwner dataOwner = DataOwner(dataOwnerCert, dataOwnerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  //==============================================

  NDN_LOG_INFO("\n=================== start work flow ==================\n");

  Name dataName = "/dataName";
  std::string policy = "(attr1 or attr2) and attr3";

  bool isPolicySet = false;
  dataOwner.commandProducerPolicy(producerCert.getIdentity(), dataName, policy,
                                   [&] (const Data& response) {
                                    NDN_LOG_DEBUG("on policy set data callback");
                                     isPolicySet = true;
                                     BOOST_CHECK_EQUAL(readString(response.getContent()), "success");
                                     auto it = producer.m_policyCache.find(dataName);
                                     BOOST_CHECK(it != producer.m_policyCache.end());
                                     BOOST_CHECK(it->second == policy);
                                   },
                                   [=] (const std::string& err) {
                                     BOOST_CHECK(false);
                                   });

  NDN_LOG_DEBUG("before policy set");
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isPolicySet);

  std::shared_ptr<Data> contentData, ckData;
  auto it = producer.m_policyCache.find(dataName);
  BOOST_CHECK(it != producer.m_policyCache.end());
  BOOST_CHECK(it->second == policy);
  std::tie(contentData, ckData) = producer.produce(dataName, it->second, PLAIN_TEXT, sizeof(PLAIN_TEXT));
  BOOST_CHECK(contentData != nullptr);
  BOOST_CHECK(ckData != nullptr);

  producerFace.setInterestFilter(producerCert.getIdentity(),
    [&] (const ndn::InterestFilter&, const ndn::Interest& interest) {
      NDN_LOG_INFO("consumer request for"<<interest.toUri());
      if (interest.getName().isPrefixOf(contentData->getName())) {
        producerFace.put(*contentData);
      }
      if (interest.getName().isPrefixOf(ckData->getName())) {
        producerFace.put(*ckData);
      }
    }
  );

  bool isConsumeCbCalled = false;
  consumer1.obtainAttributes();
  advanceClocks(time::milliseconds(20), 60);
  consumer1.consume(producerCert.getIdentity().append(dataName),
    [&] (const Buffer& result) {
      isConsumeCbCalled = true;
      BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(),
                                    PLAIN_TEXT, PLAIN_TEXT + sizeof(PLAIN_TEXT));

      std::string str;
      for(int i =0;i<sizeof(PLAIN_TEXT);++i)
        str.push_back(result[i]);
      NDN_LOG_INFO("result:"<<str);
    },
    [&] (const std::string& err) {
      BOOST_CHECK(false);
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);

  isConsumeCbCalled = false;
  consumer2.obtainAttributes();
  advanceClocks(time::milliseconds(20), 60);
  consumer2.consume(producerCert.getIdentity().append(dataName),
    [&] (const Buffer& result) {
      BOOST_CHECK(false);
    },
    [&] (const std::string& err) {
      isConsumeCbCalled = true;
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);
}

BOOST_AUTO_TEST_CASE(IntegratedTest2)
{
  // set up token issuer
  NDN_LOG_INFO("Create Token Issuer. Token Issuer prefix:"<<tokenIssuerCert.getIdentity());
  TokenIssuer tokenIssuer = TokenIssuer(tokenIssuerCert, tokenIssuerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK_EQUAL(tokenIssuer.m_interestFilterIds.size(), 1);

  // define attr list for consumer rights
  std::list<std::string> attrList = {"attr1", "attr3"};
  NDN_LOG_INFO("Add comsumer 1 "<<consumerCert1.getIdentity()<<" with attributes: attr1, attr3");
  tokenIssuer.m_tokens.insert(std::pair<Name, std::list<std::string>>(consumerCert1.getIdentity(),
                                                                      attrList));
  BOOST_CHECK_EQUAL(tokenIssuer.m_tokens.size(), 1);


  std::list<std::string> attrList1 = {"attr1"};
  NDN_LOG_INFO("Add comsumer 2 "<<consumerCert2.getIdentity()<<" with attributes: attr1");
  tokenIssuer.m_tokens.insert(std::pair<Name, std::list<std::string>>(consumerCert2.getIdentity(),
                                                                      attrList1));
  BOOST_CHECK_EQUAL(tokenIssuer.m_tokens.size(), 2);

  NDN_LOG_DEBUG("after token issuer");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace nacabe
} // namespace ndn
