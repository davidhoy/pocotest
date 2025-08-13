#!/usr/bin/env python3
"""
Comprehensive test suite for Lumitec Poco CAN API Python library
"""

import unittest
from lumitec_poco import (
    LumitecPocoAPI, PocoCANFrame, PocoActionID, PocoSwitchState, PocoSwitchType,
    PocoProprietaryID, LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE,
    create_simple_action, create_custom_hsb, create_state_info, create_start_pattern,
    is_valid_poco_frame, parse_poco_frame
)


class TestLumitecPocoAPI(unittest.TestCase):
    """Test cases for Lumitec Poco CAN API"""

    def test_version(self):
        """Test version string"""
        version = LumitecPocoAPI.get_version()
        self.assertIsNotNone(version)
        self.assertGreater(len(version), 0)
        self.assertIn(".", version)  # Should have version format like "1.0.0"

    def test_simple_action_creation(self):
        """Test simple action message creation"""
        frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
        
        self.assertEqual(frame.data_length, 6)
        self.assertEqual(frame.destination_address, 0x0E)
        self.assertEqual(frame.source_address, 0x10)
        self.assertEqual(frame.priority, 6)
        
        # Validate frame content
        self.assertTrue(is_valid_poco_frame(frame))
        
        prop_id = LumitecPocoAPI.get_proprietary_id(frame)
        self.assertEqual(prop_id, PocoProprietaryID.EXTSW_SIMPLE_ACTIONS)

    def test_simple_action_parsing(self):
        """Test simple action message parsing"""
        frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
        
        action = LumitecPocoAPI.parse_simple_action(frame)
        self.assertIsNotNone(action)
        self.assertEqual(action.action_id, PocoActionID.ON)
        self.assertEqual(action.switch_id, 1)
        self.assertEqual(action.manufacturer_code, LUMITEC_MANUFACTURER_CODE)
        self.assertEqual(action.industry_code, MARINE_INDUSTRY_CODE)

    def test_custom_hsb_creation(self):
        """Test custom HSB message creation"""
        frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 2, 128, 255, 200)
        
        self.assertEqual(frame.data_length, 8)
        self.assertTrue(is_valid_poco_frame(frame))
        
        prop_id = LumitecPocoAPI.get_proprietary_id(frame)
        self.assertEqual(prop_id, PocoProprietaryID.EXTSW_CUSTOM_HSB)

    def test_custom_hsb_parsing(self):
        """Test custom HSB message parsing"""
        frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 2, 128, 255, 200)
        
        hsb = LumitecPocoAPI.parse_custom_hsb(frame)
        self.assertIsNotNone(hsb)
        self.assertEqual(hsb.action_id, PocoActionID.T2HSB)
        self.assertEqual(hsb.switch_id, 2)
        self.assertEqual(hsb.hue, 128)
        self.assertEqual(hsb.saturation, 255)
        self.assertEqual(hsb.brightness, 200)

    def test_state_info_creation(self):
        """Test state info message creation"""
        frame = create_state_info(0x10, 3, PocoSwitchState.HELD, PocoSwitchType.LATCHING)
        
        self.assertEqual(frame.data_length, 7)
        self.assertEqual(frame.destination_address, 255)  # Broadcast
        self.assertTrue(is_valid_poco_frame(frame))

    def test_state_info_parsing(self):
        """Test state info message parsing"""
        frame = create_state_info(0x10, 3, PocoSwitchState.HELD, PocoSwitchType.LATCHING)
        
        state_info = LumitecPocoAPI.parse_state_info(frame)
        self.assertIsNotNone(state_info)
        self.assertEqual(state_info.switch_id, 3)
        self.assertEqual(state_info.switch_state, PocoSwitchState.HELD)
        self.assertEqual(state_info.switch_type, PocoSwitchType.LATCHING)

    def test_start_pattern_creation(self):
        """Test start pattern message creation"""
        frame = create_start_pattern(0x0E, 0x10, 1, 5)
        
        self.assertEqual(frame.data_length, 6)
        self.assertTrue(is_valid_poco_frame(frame))

    def test_start_pattern_parsing(self):
        """Test start pattern message parsing"""
        frame = create_start_pattern(0x0E, 0x10, 1, 5)
        
        pattern = LumitecPocoAPI.parse_start_pattern(frame)
        self.assertIsNotNone(pattern)
        self.assertEqual(pattern.switch_id, 1)
        self.assertEqual(pattern.pattern_id, 5)

    def test_string_conversions(self):
        """Test string conversion utilities"""
        self.assertEqual(LumitecPocoAPI.action_to_string(PocoActionID.ON), "On")
        self.assertEqual(LumitecPocoAPI.state_to_string(PocoSwitchState.PRESSED), "Pressed")
        self.assertEqual(LumitecPocoAPI.type_to_string(PocoSwitchType.MOMENTARY), "Momentary")
        
        # Test unknown values
        self.assertEqual(LumitecPocoAPI.action_to_string(999), "Unknown")

    def test_invalid_frames(self):
        """Test handling of invalid frames"""
        # Create frame with invalid data
        frame = PocoCANFrame()
        frame.data = b'\x00\x00'  # Too short
        
        self.assertFalse(is_valid_poco_frame(frame))
        self.assertIsNone(LumitecPocoAPI.get_proprietary_id(frame))
        
        # Frame with wrong manufacturer code
        frame.data = b'\x00\x00\x01\x02\x03\x04'
        self.assertFalse(is_valid_poco_frame(frame))

    def test_frame_data_length_validation(self):
        """Test frame data length validation"""
        with self.assertRaises(ValueError):
            PocoCANFrame(data=b'\x00' * 10)  # Too long

    def test_parse_wrong_message_type(self):
        """Test parsing messages with wrong proprietary ID"""
        frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
        
        # Should return None when parsing as wrong type
        self.assertIsNone(LumitecPocoAPI.parse_custom_hsb(frame))
        self.assertIsNone(LumitecPocoAPI.parse_state_info(frame))
        self.assertIsNone(LumitecPocoAPI.parse_start_pattern(frame))

    def test_auto_parse_function(self):
        """Test the auto-parse convenience function"""
        # Test different message types
        frames_and_types = [
            (create_simple_action(0x0E, 0x10, PocoActionID.ON, 1), "PocoSimpleAction"),
            (create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 128, 255, 200), "PocoCustomHSB"),
            (create_state_info(0x10, 1, PocoSwitchState.PRESSED, PocoSwitchType.MOMENTARY), "PocoStateInfo"),
            (create_start_pattern(0x0E, 0x10, 1, 5), "PocoStartPattern")
        ]
        
        for frame, expected_type in frames_and_types:
            parsed = parse_poco_frame(frame)
            self.assertIsNotNone(parsed)
            self.assertEqual(type(parsed).__name__, expected_type)

    def test_roundtrip_conversion(self):
        """Test that create->parse->create produces identical results"""
        # Test with custom HSB message
        original_frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 100, 150, 200)
        
        # Parse it
        hsb = LumitecPocoAPI.parse_custom_hsb(original_frame)
        self.assertIsNotNone(hsb)
        
        # Create new frame from parsed data
        new_frame = create_custom_hsb(0x0E, 0x10, PocoActionID(hsb.action_id), 
                                     hsb.switch_id, hsb.hue, hsb.saturation, hsb.brightness)
        
        # Compare frames
        self.assertEqual(original_frame.data_length, new_frame.data_length)
        self.assertEqual(original_frame.data, new_frame.data)

    def test_can_id_calculation(self):
        """Test CAN ID calculation"""
        frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
        
        # CAN ID should be non-zero and properly formatted
        self.assertGreater(frame.can_id, 0)
        self.assertLessEqual(frame.can_id, 0x1FFFFFFF)  # 29-bit CAN ID

    def test_all_action_types(self):
        """Test all action types can be created and parsed"""
        for action in PocoActionID:
            frame = create_simple_action(0x0E, 0x10, action, 1)
            parsed = LumitecPocoAPI.parse_simple_action(frame)
            self.assertIsNotNone(parsed)
            self.assertEqual(parsed.action_id, action)

    def test_boundary_values(self):
        """Test boundary values for HSB parameters"""
        # Test with minimum values
        frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 0, 0, 0)
        hsb = LumitecPocoAPI.parse_custom_hsb(frame)
        self.assertEqual(hsb.hue, 0)
        self.assertEqual(hsb.saturation, 0)
        self.assertEqual(hsb.brightness, 0)
        
        # Test with maximum values
        frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 255, 255, 255)
        hsb = LumitecPocoAPI.parse_custom_hsb(frame)
        self.assertEqual(hsb.hue, 255)
        self.assertEqual(hsb.saturation, 255)
        self.assertEqual(hsb.brightness, 255)

    def test_frame_properties(self):
        """Test PocoCANFrame properties"""
        data = b'\x01\x02\x03\x04'
        frame = PocoCANFrame(data=data)
        
        self.assertEqual(frame.data_length, 4)
        self.assertEqual(frame.data, data)


class TestPerformance(unittest.TestCase):
    """Performance tests for the API"""

    def test_creation_performance(self):
        """Test message creation performance"""
        import time
        
        start_time = time.time()
        for i in range(1000):
            create_simple_action(0x0E, 0x10, PocoActionID.ON, i % 256)
        end_time = time.time()
        
        # Should be able to create 1000 messages in reasonable time
        self.assertLess(end_time - start_time, 1.0)  # Less than 1 second

    def test_parsing_performance(self):
        """Test message parsing performance"""
        import time
        
        frames = [create_simple_action(0x0E, 0x10, PocoActionID.ON, i % 256) for i in range(100)]
        
        start_time = time.time()
        for frame in frames * 10:  # Parse each frame 10 times
            LumitecPocoAPI.parse_simple_action(frame)
        end_time = time.time()
        
        # Should be able to parse 1000 messages in reasonable time
        self.assertLess(end_time - start_time, 1.0)  # Less than 1 second


def run_tests():
    """Run all tests and return results"""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add test classes
    suite.addTests(loader.loadTestsFromTestCase(TestLumitecPocoAPI))
    suite.addTests(loader.loadTestsFromTestCase(TestPerformance))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return result.wasSuccessful()


if __name__ == "__main__":
    import sys
    
    print("Lumitec Poco CAN API Python Test Suite")
    print("=" * 50)
    
    success = run_tests()
    
    if success:
        print("\n✓ All tests passed!")
        sys.exit(0)
    else:
        print("\n✗ Some tests failed!")
        sys.exit(1)
