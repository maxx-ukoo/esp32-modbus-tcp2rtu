import React, { Component } from "react";
import Tab from 'react-bootstrap/Tab';
import Row from 'react-bootstrap/Row';
import Col from 'react-bootstrap/Col';
import Nav from 'react-bootstrap/Nav';

import DevInfo from './DevInfo.jsx'
import Modbus from './Modbus.jsx'
import Gpio from './Gpio.jsx'
import Mqtt from './Mqtt.jsx'

class Home extends Component {

    render() {
      return (
        <Tab.Container id="left-tabs-example" defaultActiveKey="info">
          <Row>
            <Col sm={2}>
              <Nav variant="pills" className="flex-column">
                <Nav.Item>
                  <Nav.Link eventKey="info">Info</Nav.Link>
                </Nav.Item>
                <Nav.Item>
                  <Nav.Link eventKey="gpio">GPIO</Nav.Link>
                </Nav.Item>
                <Nav.Item>
                  <Nav.Link eventKey="mqtt">Mqtt</Nav.Link>
                </Nav.Item>
                <Nav.Item>
                  <Nav.Link eventKey="modbus">Modbus</Nav.Link>
                </Nav.Item>
              </Nav>
            </Col>
            <Col sm={6}>
              <Tab.Content>
                <Tab.Pane eventKey="info">
                  <DevInfo />
                </Tab.Pane>
                <Tab.Pane eventKey="mqtt">
                <Mqtt />
                </Tab.Pane>
                <Tab.Pane eventKey="modbus">
                <Modbus />
                </Tab.Pane>
                <Tab.Pane eventKey="gpio">
                <Gpio />
                </Tab.Pane>
              </Tab.Content>
            </Col>
          </Row>
        </Tab.Container>
      )
    }

  }

export default Home;
